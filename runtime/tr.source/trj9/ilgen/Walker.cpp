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

#include <algorithm>
#include "codegen/CodeGenerator.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/jittypes.h"
#include "env/VMAccessCriticalSection.hpp"
#include "exceptions/AOTFailure.hpp"
#include "exceptions/FSDFailure.hpp"
#include "exceptions/CompilationAbortion.hpp"
#include "optimizer/TransformUtil.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "trj9/env/j9fieldsInfo.h"
#include "trj9/env/VMJ9.h"
#include "trj9/ilgen/ClassLookahead.hpp"
#include "trj9/ilgen/J9ByteCode.hpp"
#include "trj9/ilgen/J9ByteCodeIlGenerator.hpp"
#include "infra/Bit.hpp"               //for trailingZeroes

#define BDCLASSLEN 20
#define BDCLASS "java/math/BigDecimal"
#define BDFIELDLEN 6
#define BDFIELD "laside"
#define BDDFPHWAVAIL "java/math/BigDecimal.DFPHWAvailable()Z"
#define BDDFPHWAVAILLEN 38
#define BDDFPPERFORMHYS "java/math/BigDecimal.DFPPerformHysteresis()Z"
#define BDDFPPERFORMHYSLEN 44
#define BDDFPUSEDFP "java/math/BigDecimal.DFPUseDFP()Z"
#define BDDFPUSEDFPLEN 33

#define JAVA_SERIAL_CLASS_NAME "Ljava/io/ObjectInputStream;"
#define JAVA_SERIAL_CLASS_NAME_LEN 27
#define JAVA_SERIAL_CALLEE_METHOD_NAME_LEN 10
#define JAVA_SERIAL_CALLEE_METHOD_NAME "readObject"
#define JAVA_SERIAL_CALLEE_METHOD_SIG_LEN 20
#define JAVA_SERIAL_CALLEE_METHOD_SIG "()Ljava/lang/Object;"
#define JAVA_SERIAL_REPLACE_CLASS_LEN 25
#define JAVA_SERIAL_REPLACE_CLASS_NAME "java/io/ObjectInputStream"
#define JAVA_SERIAL_REPLACE_METHOD_SIG_LEN 64
#define JAVA_SERIAL_REPLACE_METHOD_SIG "(Ljava/io/ObjectInputStream;Ljava/lang/Class;)Ljava/lang/Object;"
#define JAVA_SERIAL_REPLACE_METHOD_NAME_LEN 20
#define JAVA_SERIAL_REPLACE_METHOD_NAME "redirectedReadObject"

#define ORB_CALLER_METHOD_NAME_LEN 10
#define ORB_CALLER_METHOD_NAME "readObject"
#define ORB_CALLER_METHOD_SIG_LEN 30
#define ORB_CALLER_METHOD_SIG "(Ljava/io/ObjectInputStream;)V"

#define ORB_CALLEE_METHOD_NAME_LEN 10
#define ORB_CALLEE_METHOD_NAME "readObject"
#define ORB_CALLEE_METHOD_SIG_LEN 20
#define ORB_CALLEE_METHOD_SIG "()Ljava/lang/Object;"

#define ORB_REPLACE_CLASS_LEN 30
#define ORB_REPLACE_CLASS_NAME "com/ibm/rmi/io/IIOPInputStream"
#define ORB_REPLACE_METHOD_SIG_LEN 64
#define ORB_REPLACE_METHOD_SIG "(Ljava/io/ObjectInputStream;Ljava/lang/Class;)Ljava/lang/Object;"
#define ORB_REPLACE_METHOD_NAME_LEN 20
#define ORB_REPLACE_METHOD_NAME "redirectedReadObject"

#define BD_DFP_HW_AVAILABLE_FLAG "DFP_HW_AVAILABLE"
#define BD_DFP_HW_AVAILABLE_FLAG_LEN 16
#define BD_DFP_HW_AVAILABLE_FLAG_SIG "Z"
#define BD_DFP_HW_AVAILABLE_FLAG_SIG_LEN 1
#define BD_DFP_HW_AVAILABLE_GET_STATIC_FIELD_ADDR_FLAG 4

#define BD_EXT_FACILITY_AVAILABLE "com/ibm/dataaccess/DecimalData.DFPFacilityAvailable()Z"
#define BD_EXT_FACILITY_AVAILABLE_LEN 57
#define BD_EXT_USE_DFP "com/ibm/dataaccess/DecimalData.DFPUseDFP()Z"
#define BD_EXT_USE_DFP_LEN 46

#define JSR292_ILGenMacros    "java/lang/invoke/ILGenMacros"
#define JSR292_placeholder    "placeholder"
#define JSR292_placeholderSig "(I)I"

#define JSR292_MethodHandle   "java/lang/invoke/MethodHandle"
#define JSR292_invokeExactTargetAddress    "invokeExactTargetAddress"
#define JSR292_invokeExactTargetAddressSig "()J"
#define JSR292_getType                     "type"
#define JSR292_getTypeSig                  "()Ljava/lang/invoke/MethodType;"

#define JSR292_invokeExact    "invokeExact"
#define JSR292_invokeExactSig "([Ljava/lang/Object;)Ljava/lang/Object;"

#define JSR292_ComputedCalls  "java/lang/invoke/ComputedCalls"
#define JSR292_dispatchDirectPrefix "dispatchDirect_"
#define JSR292_dispatchDirectArgSig "(JI)"

#define JSR292_asType              "asType"
#define JSR292_asTypeSig           "(Ljava/lang/invoke/MethodHandle;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;"
#define JSR292_forGenericInvoke    "forGenericInvoke"
#define JSR292_forGenericInvokeSig "(Ljava/lang/invoke/MethodType;Z)Ljava/lang/invoke/MethodHandle;"


TR::Node *
TR_J9ByteCodeIlGenerator::handleVectorIntrinsicCall(TR::Node *node, TR::MethodSymbol *symbol)
   {
   TR_ASSERT(TR::firstVectorIntrinsic <= symbol->getRecognizedMethod() && symbol->getRecognizedMethod() <= TR::lastVectorIntrinsic, "assertion failure");
   switch(symbol->getRecognizedMethod())
      {

      // ---- Integer Type Operations
      case TR::com_ibm_dataaccess_SIMD_vectorAddInt:
         return genVectorBinaryOp(node, TR::vadd, TR::VectorInt32, TR::iadd, TR::Int32);
      case TR::com_ibm_dataaccess_SIMD_vectorSubInt:
         return genVectorBinaryOp(node, TR::vsub, TR::VectorInt32, TR::isub, TR::Int32);
      case TR::com_ibm_dataaccess_SIMD_vectorMulInt:
         return genVectorBinaryOp(node, TR::vmul, TR::VectorInt32, TR::imul, TR::Int32);
      case TR::com_ibm_dataaccess_SIMD_vectorDivInt:
         return genVectorBinaryOp(node, TR::vdiv, TR::VectorInt32,  TR::idiv, TR::Int32);
      case TR::com_ibm_dataaccess_SIMD_vectorRemInt:
         return genVectorBinaryOp(node, TR::virem, TR::VectorInt32, TR::irem, TR::Int32);
      case TR::com_ibm_dataaccess_SIMD_vectorNegInt:
         return genVectorUnaryOp(node, TR::vneg, TR::VectorInt32, TR::ineg, TR::Int32);
      case TR::com_ibm_dataaccess_SIMD_vectorSplatsInt:
         return genVectorSplatsOp(node, TR::vsplats, TR::VectorInt32, TR::Int32);
      case TR::com_ibm_dataaccess_SIMD_vectorMinInt:
         return genVectorBinaryOp(node, TR::vimin, TR::VectorInt32, TR::imin, TR::Int32);
      case TR::com_ibm_dataaccess_SIMD_vectorMaxInt:
         return genVectorBinaryOp(node, TR::vimax, TR::VectorInt32, TR::imax, TR::Int32);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpEqInt:
	 return genVectorBinaryOp(node, TR::vicmpeq, TR::VectorInt32, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpGeInt:
        return genVectorBinaryOp(node, TR::vicmpge, TR::VectorInt32, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpGtInt:
        return genVectorBinaryOp(node, TR::vicmpgt, TR::VectorInt32, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpLeInt:
        return genVectorBinaryOp(node, TR::vicmple, TR::VectorInt32, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpLtInt:
        return genVectorBinaryOp(node, TR::vicmplt, TR::VectorInt32, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorStoreInt:
         return genVectorStore(node, TR::VectorInt32, TR::Int32);
      case TR::com_ibm_dataaccess_SIMD_vectorAndInt:
         return genVectorBinaryOp(node, TR::vand, TR::VectorInt32, TR::iand, TR::Int32);
      case TR::com_ibm_dataaccess_SIMD_vectorOrInt:
         return genVectorBinaryOp(node, TR::vor, TR::VectorInt32, TR::ior, TR::Int32);
      case TR::com_ibm_dataaccess_SIMD_vectorXorInt:
         return genVectorBinaryOp(node, TR::vxor, TR::VectorInt32, TR::ixor, TR::Int32);
      case TR::com_ibm_dataaccess_SIMD_vectorNotInt:
         return genVectorUnaryOp(node, TR::vnot, TR::VectorInt32, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorGetElementInt:
        return genVectorGetElementOp(node, TR::vigetelem);
      case TR::com_ibm_dataaccess_SIMD_vectorSetElementInt:
        return genVectorSetElementOp(node, TR::visetelem);
      case TR::com_ibm_dataaccess_SIMD_vectorMergeHighInt:
        return genVectorBinaryOp(node, TR::vimergeh, TR::VectorInt32, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorMergeLowInt:
        return genVectorBinaryOp(node, TR::vimergel, TR::VectorInt32, TR::BadILOp, TR::NoType);

      // ---- Long Type Operations
      case TR::com_ibm_dataaccess_SIMD_vectorAddLong:
         return genVectorBinaryOp(node, TR::vadd, TR::VectorInt64, TR::ladd, TR::Int64);
      case TR::com_ibm_dataaccess_SIMD_vectorSubLong:
         return genVectorBinaryOp(node, TR::vsub, TR::VectorInt64, TR::lsub, TR::Int64);
      case TR::com_ibm_dataaccess_SIMD_vectorMulLong:
         return genVectorBinaryOp(node, TR::vmul, TR::VectorInt64, TR::lmul, TR::Int64);
      case TR::com_ibm_dataaccess_SIMD_vectorDivLong:
         return genVectorBinaryOp(node, TR::vdiv, TR::VectorInt64,  TR::ldiv, TR::Int64);

      // ---- Double Type Operations
      case TR::com_ibm_dataaccess_SIMD_vectorMaddDouble:
         return genVectorTernaryOp(node, TR::vdmadd);
      case TR::com_ibm_dataaccess_SIMD_vectorNmsubDouble:
         return genVectorTernaryOp(node, TR::vdnmsub);
      case TR::com_ibm_dataaccess_SIMD_vectorMsubDouble:
         return genVectorTernaryOp(node, TR::vdmsub);
      case TR::com_ibm_dataaccess_SIMD_vectorSelDouble:
         return genVectorTernaryOp(node, TR::vdsel);
      case TR::com_ibm_dataaccess_SIMD_vectorSplatsDouble:
         return genVectorSplatsOp(node, TR::vsplats, TR::VectorDouble, TR::Double);
      case TR::com_ibm_dataaccess_SIMD_vectorStoreDouble:
         return genVectorStore(node, TR::VectorDouble, TR::Double);
      case TR::com_ibm_dataaccess_SIMD_vectorAddress:
         return genVectorAddress(node, TR::a2l);

      case TR::com_ibm_dataaccess_SIMD_vectorStoreByte:
         return genVectorStore(node, TR::VectorInt8, TR::Int8);
      case TR::com_ibm_dataaccess_SIMD_vectorStoreChar:
         return genVectorStore(node, TR::VectorInt16, TR::Int16);
      case TR::com_ibm_dataaccess_SIMD_vectorStoreShort:
         return genVectorStore(node, TR::VectorInt16, TR::Int16);
      case TR::com_ibm_dataaccess_SIMD_vectorStoreLong:
         return genVectorStore(node, TR::VectorInt64, TR::Int64);

      case TR::com_ibm_dataaccess_SIMD_vectorSplatsByte:
         return genVectorSplatsOp(node, TR::vsplats, TR::VectorInt8, TR::Int8);
      case TR::com_ibm_dataaccess_SIMD_vectorSplatsChar:
      case TR::com_ibm_dataaccess_SIMD_vectorSplatsShort:
         return genVectorSplatsOp(node, TR::vsplats, TR::VectorInt16, TR::Int16);
      case TR::com_ibm_dataaccess_SIMD_vectorSplatsLong:
         return genVectorSplatsOp(node, TR::vsplats, TR::VectorInt64, TR::Int64);

      case TR::com_ibm_dataaccess_SIMD_vectorAddFloat:
         return genVectorBinaryOp(node, TR::vadd, TR::VectorFloat, TR::fadd, TR::Float);
      case TR::com_ibm_dataaccess_SIMD_vectorSubFloat:
         return genVectorBinaryOp(node, TR::vsub, TR::VectorFloat, TR::fsub, TR::Float);
      case TR::com_ibm_dataaccess_SIMD_vectorMulFloat:
         return genVectorBinaryOp(node, TR::vmul, TR::VectorFloat, TR::fmul, TR::Float);
      case TR::com_ibm_dataaccess_SIMD_vectorDivFloat:
         return genVectorBinaryOp(node, TR::vdiv, TR::VectorFloat, TR::fdiv, TR::Float);
      case TR::com_ibm_dataaccess_SIMD_vectorNegFloat:
         return genVectorUnaryOp(node, TR::vneg, TR::VectorFloat, TR::fneg, TR::Float);
      case TR::com_ibm_dataaccess_SIMD_vectorSplatsFloat:
         return genVectorSplatsOp(node, TR::vsplats, TR::VectorFloat, TR::Float);
      case TR::com_ibm_dataaccess_SIMD_vectorAddDouble:
         return genVectorBinaryOp(node, TR::vadd, TR::VectorDouble, TR::dadd, TR::Double);
      case TR::com_ibm_dataaccess_SIMD_vectorMinDouble:
         return genVectorBinaryOp(node, TR::vdmin, TR::VectorDouble, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorMaxDouble:
         return genVectorBinaryOp(node, TR::vdmax, TR::VectorDouble, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorDivDouble:
         return genVectorBinaryOp(node, TR::vdiv, TR::VectorDouble, TR::ddiv, TR::Double);
      case TR::com_ibm_dataaccess_SIMD_vectorMulDouble:
         return genVectorBinaryOp(node, TR::vmul, TR::VectorDouble, TR::dmul, TR::Double);
      case TR::com_ibm_dataaccess_SIMD_vectorSubDouble:
         return genVectorBinaryOp(node, TR::vsub, TR::VectorDouble, TR::dsub, TR::Double);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAllEqInt:
        return genVectorBinaryOpNoTrg(node, TR::vicmpalleq);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAllGeInt:
        return genVectorBinaryOpNoTrg(node, TR::vicmpallge);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAllGtInt:
        return genVectorBinaryOpNoTrg(node, TR::vicmpallgt);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAllLeInt:
        return genVectorBinaryOpNoTrg(node, TR::vicmpallle);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAllLtInt:
        return genVectorBinaryOpNoTrg(node, TR::vicmpalllt);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAnyEqInt:
        return genVectorBinaryOpNoTrg(node, TR::vicmpanyeq);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAnyGeInt:
        return genVectorBinaryOpNoTrg(node, TR::vicmpanyge);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAnyGtInt:
        return genVectorBinaryOpNoTrg(node, TR::vicmpanygt);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAnyLeInt:
        return genVectorBinaryOpNoTrg(node, TR::vicmpanyle);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAnyLtInt:
        return genVectorBinaryOpNoTrg(node, TR::vicmpanylt);
      // case TR::com_ibm_dataaccess_SIMD_vectorRemDouble:
         // return genVectorBinaryOp(node, TR::vdrem);
      case TR::com_ibm_dataaccess_SIMD_vectorNegDouble:
         return genVectorUnaryOp(node, TR::vneg, TR::VectorDouble, TR::dneg, TR::Double);
      case TR::com_ibm_dataaccess_SIMD_vectorSqrtDouble:
         return genVectorUnaryOp(node, TR::vdsqrt, TR::VectorDouble, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorMergeHighDouble:
        return genVectorBinaryOp(node, TR::vdmergeh, TR::VectorDouble, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorMergeLowDouble:
        return genVectorBinaryOp(node, TR::vdmergel, TR::VectorDouble, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpEqDouble:
        return genVectorBinaryOp(node, TR::vdcmpeq, TR::VectorDouble, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpGeDouble:
        return genVectorBinaryOp(node, TR::vdcmpge, TR::VectorDouble, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpGtDouble:
        return genVectorBinaryOp(node, TR::vdcmpgt, TR::VectorDouble, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpLeDouble:
        return genVectorBinaryOp(node, TR::vdcmple, TR::VectorDouble, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpLtDouble:
        return genVectorBinaryOp(node, TR::vdcmplt, TR::VectorDouble, TR::BadILOp, TR::NoType);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAllEqDouble:
        return genVectorBinaryOpNoTrg(node, TR::vdcmpalleq);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAllGeDouble:
        return genVectorBinaryOpNoTrg(node, TR::vdcmpallge);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAllGtDouble:
        return genVectorBinaryOpNoTrg(node, TR::vdcmpallgt);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAllLeDouble:
        return genVectorBinaryOpNoTrg(node, TR::vdcmpallle);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAllLtDouble:
        return genVectorBinaryOpNoTrg(node, TR::vdcmpalllt);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAnyEqDouble:
        return genVectorBinaryOpNoTrg(node, TR::vdcmpanyeq);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAnyGeDouble:
        return genVectorBinaryOpNoTrg(node, TR::vdcmpanyge);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAnyGtDouble:
        return genVectorBinaryOpNoTrg(node, TR::vdcmpanygt);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAnyLeDouble:
        return genVectorBinaryOpNoTrg(node, TR::vdcmpanyle);
      case TR::com_ibm_dataaccess_SIMD_vectorCmpAnyLtDouble:
        return genVectorBinaryOpNoTrg(node, TR::vdcmpanylt);
      case TR::com_ibm_dataaccess_SIMD_vectorGetElementDouble:
        return genVectorGetElementOp(node, TR::vdgetelem);
      case TR::com_ibm_dataaccess_SIMD_vectorSetElementDouble:
        return genVectorSetElementOp(node, TR::vdsetelem);
      case TR::com_ibm_dataaccess_SIMD_vectorAddReduceDouble:
        return genVectorAddReduceDouble(node);
      case TR::com_ibm_dataaccess_SIMD_vectorLoadWithStrideDouble:
        return genVectorLoadWithStrideDouble(node);
      case TR::com_ibm_dataaccess_SIMD_vectorLogDouble:
	 return genVectorUnaryOp(node, TR::vdlog, TR::VectorDouble, TR::BadILOp, TR::NoType);


      default:
      	NULL;
      }

   return NULL;
   }


TR::Node *
TR_J9ByteCodeIlGenerator::genVectorUnaryOp(TR::Node *node, TR::ILOpCodes op1, TR::DataType dt1, TR::ILOpCodes op2, TR::DataType dt2)
   {
   if (comp()->getOption(TR_TraceILGen)) traceMsg(comp(), "Vector intrinsic method call recognized in %p\n", node);

   TR::Node *dst_idx = node->getChild(1);
   TR::Node *src1_idx = node->getChild(3);

   TR::Node *valueNode;
   if (comp()->getOption(TR_EnableSIMDLibrary))
      {
      push(node->getChild(2));
      push(src1_idx);
      loadArrayElement(dt1);
      TR::Node *firstOperandNode = pop();

      push(node->getChild(0));
      push(dst_idx);

      valueNode = TR::Node::create(op1, 1);
      valueNode->setAndIncChild(0, firstOperandNode);
      push(valueNode);
      storeArrayElement(dt1);
      }
   else if (op2 != TR::BadILOp)
      {
      TR_ASSERT(dt2 == TR::Double, "type not supported\n");

      push(node->getChild(2));
      push(src1_idx);
      loadArrayElement(dt2);
      TR::Node *firstOperandNode = pop();

      push(node->getChild(0));
      push(dst_idx);

      valueNode = TR::Node::create(op2, 1);
      valueNode->setAndIncChild(0, firstOperandNode);
      push(valueNode);
      storeArrayElement(dt2);

      // handle second element
      TR::Node *one = TR::Node::create(TR::iconst, 0, 1);

      TR::Node *dst_idx1 = TR::Node::create(TR::iadd, 2);
      dst_idx1->setAndIncChild(0, dst_idx);
      dst_idx1->setAndIncChild(1, one);

      TR::Node *src1_idx1 = TR::Node::create(TR::iadd, 2);
      src1_idx1->setAndIncChild(0, src1_idx);
      src1_idx1->setAndIncChild(1, one);

      push(node->getChild(2));
      push(src1_idx1);
      loadArrayElement(dt2);
      firstOperandNode = pop();

      push(node->getChild(0));
      push(dst_idx1);

      valueNode = TR::Node::create(op2, 1);
      valueNode->setAndIncChild(0, firstOperandNode);
      push(valueNode);
      storeArrayElement(dt2);
      }
   else
      {
      return NULL;
      }

   for (int i = 0; i < node->getNumChildren(); i++)
       node->getChild(i)->recursivelyDecReferenceCount();

   return valueNode;
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genVectorAddress(TR::Node *node, TR::ILOpCodes op)
   {
   if (!comp()->getOption(TR_EnableSIMDLibrary))
      return NULL;

   if (comp()->getOption(TR_TraceILGen)) traceMsg(comp(), "Vector intrinsic method call recognized in %p\n", node);

   TR::Node *firstOperandNode = node->getChild(0);

   TR::Node *valueNode = TR::Node::create(op, 1);
   valueNode->setAndIncChild(0, firstOperandNode);

   for (int i = 0; i < node->getNumChildren(); i++)
       node->getChild(i)->recursivelyDecReferenceCount();

   return valueNode;
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genVectorBinaryOpNoTrg(TR::Node *node, TR::ILOpCodes op)
   {
   if (comp()->getOption(TR_TraceILGen)) traceMsg(comp(), "Vector intrinsic method call recognized in %p\n", node);

   TR::DataType operandType = TR::NoType;
   switch(op)
      {
      case TR::vicmpalleq: case TR::vicmpallgt: case TR::vicmpallge: case TR::vicmpalllt: case TR::vicmpallle:
      case TR::vicmpanyeq: case TR::vicmpanygt: case TR::vicmpanyge: case TR::vicmpanylt: case TR::vicmpanyle:
         operandType = TR::VectorInt32;
         break;
      case TR::vdcmpalleq: case TR::vdcmpallgt: case TR::vdcmpallge: case TR::vdcmpalllt: case TR::vdcmpallle:
      case TR::vdcmpanyeq: case TR::vdcmpanygt: case TR::vdcmpanyge: case TR::vdcmpanylt: case TR::vdcmpanyle:
         operandType = TR::VectorDouble;
         break;
      default:
         TR_ASSERT(0, "Unexpexted operation.\n");
      }

   push(node->getChild(0));
   push(node->getChild(1));
   loadArrayElement(operandType);
   TR::Node *firstOperandNode = pop();

   push(node->getChild(2));
   push(node->getChild(3));
   loadArrayElement(operandType);
   TR::Node *secondOperandNode = pop();

   TR::Node *valueNode = TR::Node::create(op, 2);
   valueNode->setAndIncChild(0, firstOperandNode);
   valueNode->setAndIncChild(1, secondOperandNode);
   push(valueNode);

   for (int i = 0; i < node->getNumChildren(); i++)
       node->getChild(i)->recursivelyDecReferenceCount();

   return valueNode;
   }


TR::Node *
TR_J9ByteCodeIlGenerator::genVectorBinaryOp(TR::Node *node, TR::ILOpCodes op1, TR::DataType dt1, TR::ILOpCodes op2, TR::DataType dt2)
   {
   if (comp()->getOption(TR_TraceILGen)) traceMsg(comp(), "Vector intrinsic method call recognized in %p\n", node);

   if (!comp()->cg()->getSupportsOpCodeForAutoSIMD(op1, dt2))
      {
      printf("SIMD opcode is not supprted on this platform\n");
      return NULL;
      }

   TR::Node *dst_idx = node->getChild(1);
   TR::Node *src1_idx = node->getChild(3);
   TR::Node *src2_idx = node->getChild(5);

   TR::DataType vectorType = dt1;

   TR::Node *valueNode;
   if (comp()->getOption(TR_EnableSIMDLibrary))
      {
      push(node->getChild(2));
      push(src1_idx);
      loadArrayElement(vectorType);
      TR::Node *firstOperandNode = pop();

      push(node->getChild(4));
      push(src2_idx);
      loadArrayElement(vectorType);
      TR::Node *secondOperandNode = pop();

      push(node->getChild(0));
      push(dst_idx);

      valueNode = TR::Node::create(op1, 2);
      valueNode->setAndIncChild(0, firstOperandNode);
      valueNode->setAndIncChild(1, secondOperandNode);
      push(valueNode);

      TR_ASSERT((valueNode->getDataType() == vectorType) || valueNode->getOpCode().isBooleanCompare(), "Data Type mismatching %s\n",valueNode->getDataType().toString());

      storeArrayElement(vectorType);
      }
   else if (op2 != TR::BadILOp)
      {
      TR::DataType scalarType = dt2;

      TR_ASSERT(scalarType == TR::Double || scalarType == TR::Int32, "type not supported\n");

      push(node->getChild(2));
      push(src1_idx);
      loadArrayElement(scalarType);
      TR::Node *firstOperandNode = pop();

      push(node->getChild(4));
      push(src2_idx);
      loadArrayElement(scalarType);
      TR::Node *secondOperandNode = pop();

      push(node->getChild(0));
      push(dst_idx);

      valueNode = TR::Node::create(op2, 2);
      valueNode->setAndIncChild(0, firstOperandNode);
      valueNode->setAndIncChild(1, secondOperandNode);
      push(valueNode);
      storeArrayElement(scalarType);

      // handle the rest of the elements
      TR::Node *one = TR::Node::create(TR::iconst, 0, 1);
      int N = (scalarType == TR::Double) ? 1 : 3;

      for (int i = 0; i < N; i++)
         {
         TR::Node *dst_idx1 = TR::Node::create(TR::iadd, 2);
         dst_idx1->setAndIncChild(0, dst_idx);
         dst_idx1->setAndIncChild(1, one);

         TR::Node *src1_idx1 = TR::Node::create(TR::iadd, 2);
         src1_idx1->setAndIncChild(0, src1_idx);
         src1_idx1->setAndIncChild(1, one);

         TR::Node *src2_idx1 = TR::Node::create(TR::iadd, 2);
         src2_idx1->setAndIncChild(0, src2_idx);
         src2_idx1->setAndIncChild(1, one);

         push(node->getChild(2));
         push(src1_idx1);
         loadArrayElement(scalarType);
         firstOperandNode = pop();

         push(node->getChild(4));
         push(src2_idx1);
         loadArrayElement(scalarType);
         secondOperandNode = pop();

         push(node->getChild(0));
         push(dst_idx1);

         valueNode = TR::Node::create(op2, 2);
         valueNode->setAndIncChild(0, firstOperandNode);
         valueNode->setAndIncChild(1, secondOperandNode);
         push(valueNode);
         storeArrayElement(scalarType);

         dst_idx = dst_idx1;
         src1_idx = src1_idx1;
         src2_idx = src2_idx1;
         }
      }
   else if (op1 == TR::vdmergeh || op1 == TR::vdmergel)
      {
      TR::Node *one = TR::Node::create(TR::iconst, 0, 1);

      TR::Node *dst_idx1 = TR::Node::create(TR::iadd, 2);
      dst_idx1->setAndIncChild(0, dst_idx);
      dst_idx1->setAndIncChild(1, one);

      if (op1 == TR::vdmergel)
         {
         TR::Node *src1_idx1 = TR::Node::create(TR::iadd, 2);
         src1_idx1->setAndIncChild(0, src1_idx);
         src1_idx1->setAndIncChild(1, one);
         src1_idx = src1_idx1;

         TR::Node *src2_idx1 = TR::Node::create(TR::iadd, 2);
         src2_idx1->setAndIncChild(0, src2_idx);
         src2_idx1->setAndIncChild(1, one);
         src2_idx = src2_idx1;
         }

      push(node->getChild(0));
      push(dst_idx);
      push(node->getChild(2));
      push(src1_idx);
      loadArrayElement(TR::Double);
      storeArrayElement(TR::Double);

      push(node->getChild(0));
      push(dst_idx1);
      push(node->getChild(4));
      push(src2_idx);
      loadArrayElement(TR::Double);
      storeArrayElement(TR::Double);

      valueNode = src2_idx;
      }
   else
      {
      return NULL;
      }

   for (int i = 0; i < node->getNumChildren(); i++)
       node->getChild(i)->recursivelyDecReferenceCount();

   return valueNode;
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genVectorTernaryOp(TR::Node *node, TR::ILOpCodes op)
   {
   if (comp()->getOption(TR_TraceILGen)) traceMsg(comp(), "Vector intrinsic method call recognized in %p\n", node);

   TR::Node *dst_idx = node->getChild(1);
   TR::Node *src1_idx = node->getChild(3);
   TR::Node *src2_idx = node->getChild(5);
   TR::Node *src3_idx = node->getChild(7);

   TR::Node *valueNode;
   if (comp()->getOption(TR_EnableSIMDLibrary))
      {
      push(node->getChild(2));
      push(src1_idx);
      loadArrayElement(TR::VectorDouble);
      TR::Node *firstOperandNode = pop();

      push(node->getChild(4));
      push(src2_idx);
      loadArrayElement(TR::VectorDouble);
      TR::Node *secondOperandNode = pop();

      push(node->getChild(6));
      push(src3_idx);
      loadArrayElement(TR::VectorDouble);
      TR::Node *thirdOperandNode = pop();

      push(node->getChild(0));
      push(dst_idx);

      valueNode = TR::Node::create(op, 3);
      valueNode->setAndIncChild(0, firstOperandNode);
      valueNode->setAndIncChild(1, secondOperandNode);
      valueNode->setAndIncChild(2, thirdOperandNode);
      push(valueNode);

      storeArrayElement(TR::VectorDouble);
      }
   else if (op == TR::vdmadd || op == TR::vdmsub || op == TR::vdnmsub)
      {
      push(node->getChild(2));
      push(src1_idx);
      loadArrayElement(TR::Double);
      TR::Node *firstOperandNode = pop();

      push(node->getChild(4));
      push(src2_idx);
      loadArrayElement(TR::Double);
      TR::Node *secondOperandNode = pop();

      push(node->getChild(6));
      push(src3_idx);
      loadArrayElement(TR::Double);
      TR::Node *thirdOperandNode = pop();

      push(node->getChild(0));
      push(dst_idx);

      TR::ILOpCodes op2;

      if (op == TR::vdmadd)
         op2 = TR::dadd;
      else if (op == TR::vdmsub || TR::vdnmsub)
         op2 = TR::dsub;

      valueNode = TR::Node::create(TR::dmul, 2);
      valueNode->setAndIncChild(0, firstOperandNode);
      valueNode->setAndIncChild(1, secondOperandNode);
      firstOperandNode = valueNode;

      valueNode = TR::Node::create(op2, 2);
      valueNode->setAndIncChild(0, op == TR::vdnmsub ? thirdOperandNode : firstOperandNode);
      valueNode->setAndIncChild(1, op == TR::vdnmsub ? firstOperandNode : thirdOperandNode);
      push(valueNode);
      storeArrayElement(TR::Double);

      // handle second element
      TR::Node *one = TR::Node::create(TR::iconst, 0, 1);
      TR::Node *dst_idx1 = TR::Node::create(TR::iadd, 2);
      dst_idx1->setAndIncChild(0, dst_idx);
      dst_idx1->setAndIncChild(1, one);

      TR::Node *src1_idx1 = TR::Node::create(TR::iadd, 2);
      src1_idx1->setAndIncChild(0, src1_idx);
      src1_idx1->setAndIncChild(1, one);

      TR::Node *src2_idx1 = TR::Node::create(TR::iadd, 2);
      src2_idx1->setAndIncChild(0, src2_idx);
      src2_idx1->setAndIncChild(1, one);

      TR::Node *src3_idx1 = TR::Node::create(TR::iadd, 2);
      src3_idx1->setAndIncChild(0, src3_idx);
      src3_idx1->setAndIncChild(1, one);

      push(node->getChild(2));
      push(src1_idx1);
      loadArrayElement(TR::Double);
      firstOperandNode = pop();

      push(node->getChild(4));
      push(src2_idx1);
      loadArrayElement(TR::Double);
      secondOperandNode = pop();

      push(node->getChild(6));
      push(src3_idx1);
      loadArrayElement(TR::Double);
      thirdOperandNode = pop();

      push(node->getChild(0));
      push(dst_idx1);

      valueNode = TR::Node::create(TR::dmul, 2);
      valueNode->setAndIncChild(0, firstOperandNode);
      valueNode->setAndIncChild(1, secondOperandNode);
      firstOperandNode = valueNode;

      valueNode = TR::Node::create(op2, 2);
      valueNode->setAndIncChild(0, op == TR::vdnmsub ? thirdOperandNode : firstOperandNode);
      valueNode->setAndIncChild(1, op == TR::vdnmsub ? firstOperandNode : thirdOperandNode);
      push(valueNode);
      storeArrayElement(TR::Double);
      }
   else
      {
      return NULL;
      }

   for (int i = 0; i < node->getNumChildren(); i++)
       node->getChild(i)->recursivelyDecReferenceCount();

   return valueNode;
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genVectorSplatsOp(TR::Node *node, TR::ILOpCodes op, TR::DataType vectorType, TR::DataType scalarType)
   {
   if (comp()->getOption(TR_TraceILGen))
      traceMsg(comp(), "Vector intrinsic method call recognized in %p\n", node);

   TR::Node *dst_idx = node->getChild(1);
   TR::Node *baseAddr = node->getChild(0);
   TR::Node *value = node->getChild(2);
   TR::Node *valueNode;

   if (comp()->getOption(TR_EnableSIMDLibrary))
      {
      push(baseAddr);
      push(dst_idx);

      valueNode = TR::Node::create(op, 1);

      if (value->getDataType() != scalarType)
         value = TR::Node::create(TR::ILOpCode::getProperConversion(value->getDataType(), scalarType, false), 1, value);

      valueNode->setAndIncChild(0, value);

      push(valueNode);
      storeArrayElement(vectorType);
      }
   else
      {
      TR_ASSERT(scalarType == TR::Double || scalarType == TR::Int32, "type not supported\n");

      push(baseAddr);
      push(dst_idx);
      valueNode = value;
      push(valueNode);
      storeArrayElement(scalarType);

      // handle the rest of the elements
      int N = (scalarType == TR::Double) ? 1 : 3;
      TR::Node *one = TR::Node::create(TR::iconst, 0, 1);

      for (int i = 0; i < N; i++)
         {
         TR::Node *dst_idx1 = TR::Node::create(TR::iadd, 2);
         dst_idx1->setAndIncChild(0, dst_idx);
         dst_idx1->setAndIncChild(1, one);

         push(baseAddr);
         push(dst_idx1);
         push(valueNode);
         storeArrayElement(scalarType);

         dst_idx = dst_idx1;
         }
      }

   for (int i = 0; i < node->getNumChildren(); i++)
       node->getChild(i)->recursivelyDecReferenceCount();

   return valueNode;
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genVectorStore(TR::Node *node, TR::DataType dt, TR::DataType dt2)
   {
   if (comp()->getOption(TR_TraceILGen))
      traceMsg(comp(), "Vector intrinsic method call recognized in %p\n", node);

   TR::Node *dst_idx = node->getChild(1);
   TR::Node *src_idx = node->getChild(3);

   TR::Node *value;
   if (comp()->getOption(TR_EnableSIMDLibrary))
      {
      push(node->getChild(0));
      push(dst_idx);

      push(node->getChild(2));
      push(src_idx);
      loadArrayElement(dt);
      value = pop();
      push(value);
      storeArrayElement(dt);
      }
   else
      {
      push(node->getChild(0));
      push(dst_idx);
      push(node->getChild(2));
      push(src_idx);
      loadArrayElement(dt2);
      value = pop();
      push(value);
      storeArrayElement(dt2);

      // handle the rest of the elements
      TR_ASSERT(dt2 == TR::Double || dt2 == TR::Int32, "Not supported data type\n");
      int N = (dt2 == TR::Double) ? 1 : 3;
      TR::Node *one = TR::Node::create(TR::iconst, 0, 1);

      for (int i = 0; i < N; i++)
         {
         TR::Node *dst_idx1 = TR::Node::create(TR::iadd, 2);
         dst_idx1->setAndIncChild(0, dst_idx);
         dst_idx1->setAndIncChild(1, one);

         TR::Node *src_idx1 = TR::Node::create(TR::iadd, 2);
         src_idx1->setAndIncChild(0, src_idx);
         src_idx1->setAndIncChild(1, one);

         push(node->getChild(0));
         push(dst_idx1);
         push(node->getChild(2));
         push(src_idx1);
         loadArrayElement(dt2);
         value = pop();
         push(value);
         storeArrayElement(dt2);

         dst_idx = dst_idx1;
         src_idx = src_idx1;
         }
      }

   for (int i = 0; i < node->getNumChildren(); i++)
       node->getChild(i)->recursivelyDecReferenceCount();

   return value;
   }


TR::Node *
TR_J9ByteCodeIlGenerator::genVectorGetElementOp(TR::Node *node, TR::ILOpCodes op)
   {
   if (comp()->getOption(TR_TraceILGen)) traceMsg(comp(), "Vector intrinsic method call recognized in %p\n", node);

   TR::Node *valueNode;
   if (comp()->getOption(TR_EnableSIMDLibrary))
      {
      push(node->getChild(0));
      loadConstant(TR::iconst, 0);
      loadArrayElement(op == TR::vigetelem ? TR::VectorInt32 : TR::VectorDouble);
      TR::Node *firstOperandNode = pop();

      valueNode = TR::Node::create(op, 2);
      valueNode->setAndIncChild(0, firstOperandNode);
      valueNode->setAndIncChild(1, node->getChild(1));
      }
   else
      {
      push(node->getChild(0));
      push(node->getChild(1));
      loadArrayElement(op == TR::vigetelem ? TR::Int32 : TR::Double);
      valueNode = pop();
      }

   for (int i = 0; i < node->getNumChildren(); i++)
       node->getChild(i)->recursivelyDecReferenceCount();

   return valueNode;
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genVectorSetElementOp(TR::Node *node, TR::ILOpCodes op)
   {
   if (comp()->getOption(TR_TraceILGen)) traceMsg(comp(), "Vector intrinsic method call recognized in %p\n", node);

   push(node->getChild(0));
   loadConstant(TR::iconst, 0);

   push(node->getChild(0));
   loadConstant(TR::iconst, 0);
   loadArrayElement(op == TR::visetelem ?  TR::VectorInt32 : TR::VectorDouble);
   TR::Node *firstOperandNode = pop();

   TR::Node *valueNode = TR::Node::create(op, 3);
   valueNode->setAndIncChild(0, firstOperandNode);
   valueNode->setAndIncChild(1, node->getChild(1));
   valueNode->setAndIncChild(2, node->getChild(2));

   push(valueNode);
   storeArrayElement(op == TR::visetelem ?  TR::VectorInt32 : TR::VectorDouble);

   for (int i = 0; i < node->getNumChildren(); i++)
       node->getChild(i)->recursivelyDecReferenceCount();

   return valueNode;
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genVectorAddReduceDouble(TR::Node *node)
   {
   if (comp()->getOption(TR_TraceILGen)) traceMsg(comp(), "Vector intrinsic method call recognized in %p\n", node);

   TR::Node *src_idx = node->getChild(1);
   TR::Node *valueNode;
   if (comp()->getOption(TR_EnableSIMDLibrary))
      {
      push(node->getChild(0));
      push(src_idx);
      loadArrayElement(TR::VectorDouble);
      TR::Node *vectorNode = pop();

      loadConstant(TR::iconst, 0);
      TR::Node *const0 = pop();
      loadConstant(TR::iconst, 1);
      TR::Node *const1 = pop();

      TR::Node *elem0 = TR::Node::create(TR::vdgetelem, 2);
      elem0->setAndIncChild(0, vectorNode);
      elem0->setAndIncChild(1, const0);

      TR::Node *elem1 = TR::Node::create(TR::vdgetelem, 2);
      elem1->setAndIncChild(0, vectorNode);
      elem1->setAndIncChild(1, const1);

      valueNode = TR::Node::create(TR::dadd, 2);
      valueNode->setAndIncChild(0, elem0);
      valueNode->setAndIncChild(1, elem1);
      }
   else
      {
      push(node->getChild(0));
      push(src_idx);
      loadArrayElement(TR::Double);
      TR::Node *elem0 = pop();

      loadConstant(TR::iconst, 1);
      TR::Node *const1 = pop();
      TR::Node *src_idx1 = TR::Node::create(TR::iadd, 2);
      src_idx1->setAndIncChild(0, src_idx);
      src_idx1->setAndIncChild(1, const1);

      push(node->getChild(0));
      push(src_idx1);
      loadArrayElement(TR::Double);
      TR::Node *elem1 = pop();

      valueNode = TR::Node::create(TR::dadd, 2);
      valueNode->setAndIncChild(0, elem0);
      valueNode->setAndIncChild(1, elem1);
      }

   for (int i = 0; i < node->getNumChildren(); i++)
       node->getChild(i)->recursivelyDecReferenceCount();

   return valueNode;
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genVectorLoadWithStrideDouble(TR::Node *node)
   {
   if (comp()->getOption(TR_TraceILGen)) traceMsg(comp(), "Vector intrinsic method call recognized in %p\n", node);

   TR::Node *dst_idx = node->getChild(1);
   TR::Node *src_idx = node->getChild(3);
   TR::Node *stride = node->getChild(4);

   TR::Node *valueNode;
   if (comp()->getOption(TR_EnableSIMDLibrary))
      {
      push(node->getChild(0));
      push(dst_idx);
      loadArrayElement(TR::VectorDouble);
      TR::Node *vectorNode = pop();

      push(node->getChild(2));
      push(src_idx);
      loadArrayElement(TR::Double);
      TR::Node *doubleNode = pop();

      loadConstant(TR::iconst, 0);
      TR::Node *const0 = pop();
      loadConstant(TR::iconst, 1);
      TR::Node *const1 = pop();

      valueNode = TR::Node::create(TR::vdsetelem, 3);
      valueNode->setAndIncChild(0, vectorNode);
      valueNode->setAndIncChild(1, doubleNode);
      valueNode->setAndIncChild(2, const0);

      vectorNode = valueNode;

      TR::Node *idxNode = TR::Node::create(TR::iadd, 2);
      idxNode->setAndIncChild(0, src_idx);
      idxNode->setAndIncChild(1, stride);

      push(node->getChild(2));
      push(idxNode);
      loadArrayElement(TR::Double);
      doubleNode = pop();

      valueNode = TR::Node::create(TR::vdsetelem, 3);
      valueNode->setAndIncChild(0, vectorNode);
      valueNode->setAndIncChild(1, doubleNode);
      valueNode->setAndIncChild(2, const1);

      push(node->getChild(0));
      push(dst_idx);
      push(valueNode);
      storeArrayElement(TR::VectorDouble);
      }
   else
      {
      push(node->getChild(0));
      push(dst_idx);
      push(node->getChild(2));
      push(src_idx);
      loadArrayElement(TR::Double);
      storeArrayElement(TR::Double);

      loadConstant(TR::iconst, 1);
      TR::Node *const1 = pop();
      TR::Node *dst_idx1 = TR::Node::create(TR::iadd, 2);
      dst_idx1->setAndIncChild(0, dst_idx);
      dst_idx1->setAndIncChild(1, const1);

      TR::Node *src_idx1 = TR::Node::create(TR::iadd, 2);
      src_idx1->setAndIncChild(0, src_idx);
      src_idx1->setAndIncChild(1, stride);

      push(node->getChild(0));
      push(dst_idx1);
      push(node->getChild(2));
      push(src_idx1);
      loadArrayElement(TR::Double);
      storeArrayElement(TR::Double);
      valueNode = src_idx1;
      }

   for (int i = 0; i < node->getNumChildren(); i++)
       node->getChild(i)->recursivelyDecReferenceCount();

   return valueNode;

   }


// find CPindex for "element0" field
static int32_t getCPIndexForVectorElement(TR::Compilation *comp, TR::ResolvedMethodSymbol *method)
   {
   TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());
   int cp = 0;
   while (true)
      {
      int len;
      char *fieldName = method->getResolvedMethod()->fieldNameChars(cp,len);
      if (len == 8 && 0 == strncmp(fieldName, "element0", 8)) break;
      cp++;
      }
   return cp;
   }


static void printStack(TR::Compilation *comp, TR_Stack<TR::Node*> *stack, const char *message)
   {
   // TODO: This should be in the debug DLL
   if (stack->isEmpty())
      {
      traceMsg(comp, "   ---- %s: empty -----------------\n", message);
      }
   else
      {
      TR_BitVector nodesAlreadyPrinted(comp->getNodeCount(), comp->trMemory(), stackAlloc, growable);
      comp->getDebug()->saveNodeChecklist(nodesAlreadyPrinted);
      char buf[30];
      traceMsg(comp, "   /--- %s ------------------------", message);
      for (int i = stack->topIndex(); i >= 0; --i)
         {
         TR::Node *node = stack->element(i);
         traceMsg(comp, "\n");
         sprintf(buf, "   @%-2d", i);
         comp->getDebug()->printWithFixedPrefix(comp->getOutFile(), node, 1, false, true, buf);
         if (!nodesAlreadyPrinted.isSet(node->getGlobalIndex()))
            {
            for (int j = 0; j < node->getNumChildren(); ++j)
               {
               traceMsg(comp, "\n");
               comp->getDebug()->printWithFixedPrefix(comp->getOutFile(), node->getChild(j), 3, true, true, "      ");
               }
            }
         }
      traceMsg(comp, "\n");
      }
   }

static void printTrees(TR::Compilation *comp, TR::TreeTop *firstTree, TR::TreeTop *stopTree, const char *message)
   {
   // TODO: This should be in the debug DLL
   if (firstTree == stopTree)
      {
      traceMsg(comp, "   ---- %s: none ------------------\n", message);
      }
   else
      {
      traceMsg(comp, "   /--- %s ------------------------", message);
      for (TR::TreeTop *tt = firstTree; tt && tt != stopTree; tt = tt->getNextTreeTop())
         {
         traceMsg(comp, "\n");
         comp->getDebug()->printWithFixedPrefix(comp->getOutFile(), tt->getNode(), 1, true, true, "      ");
         }
      traceMsg(comp, "\n");
      }
   }

static TR::ILOpCodes getCallOpForType(TR::DataType type)
   {
   switch(type)
      {
      case TR::Address: return TR::acall;
      case TR::Float : return TR::fcall;
      case TR::Double: return TR::dcall;
      case TR::Int32:
      case TR::Int16:
      case TR::Int8: return TR::icall;
      case TR::Int64:return TR::lcall;
      default: TR_ASSERT(false, "assertion failure");
      }
   return TR::BadILOp;
   }

#define AMRCLASSLEN 51
#define AMRCLASS "java/util/concurrent/atomic/AtomicMarkableReference"
#define AMRDCASAVAIL "java/util/concurrent/atomic/AtomicMarkableReference.doubleWordCASSupported()Z"
#define AMRDCASAVAILLEN 77
#define AMRDSETAVAIL "java/util/concurrent/atomic/AtomicMarkableReference.doubleWordSetSupported()Z"
#define AMRDSETAVAILLEN 77

#define ASRCLASSLEN 50
#define ASRCLASS "java/util/concurrent/atomic/AtomicStampedReference"
#define ASRDCASAVAIL "java/util/concurrent/atomic/AtomicStampedReference.doubleWordCASSupported()Z"
#define ASRDCASAVAILLEN 76
#define ASRDSETAVAIL "java/util/concurrent/atomic/AtomicStampedReference.doubleWordSetSupported()Z"
#define ASRDSETAVAILLEN 76

#define DCAS_AVAILABLE_FLAG "dWordCASSupported"
#define DCAS_AVAILABLE_FLAG_LEN 17
#define DCAS_AVAILABLE_FLAG_SIG "Z"
#define DCAS_AVAILABLE_FLAG_SIG_LEN 1

#define DSET_AVAILABLE_FLAG "dWordSetSupported"
#define DSET_AVAILABLE_FLAG_LEN 17
#define DSET_AVAILABLE_FLAG_SIG "Z"
#define DSET_AVAILABLE_FLAG_SIG_LEN 1

TR::Block * TR_J9ByteCodeIlGenerator::walker(TR::Block * prevBlock)
   {
   int32_t i, lastIndex = _bcIndex, firstIndex = _bcIndex;

   if (comp()->getOption(TR_TraceILGen))
      {
      comp()->getDebug()->clearNodeChecklist();
      traceMsg(comp(), "==== Starting ILGen walker at bytecode %x", _bcIndex);
      if (_argPlaceholderSlot != -1)
         traceMsg(comp(), " argPlaceholderSlot=%d", _argPlaceholderSlot);
      traceMsg(comp(), "\n");
      }

   while (_bcIndex < _maxByteCodeIndex)
      {
      if (blocks(_bcIndex) && blocks(_bcIndex) != _block)
         {
         if (isGenerated(_bcIndex))
            _bcIndex = genGoto(_bcIndex);
         else
            _bcIndex = genBBEndAndBBStart();
         if (_bcIndex >= _maxByteCodeIndex)
            break;
         }

      if (_bcIndex < firstIndex)
         firstIndex = _bcIndex;
      else if (_bcIndex > lastIndex)
         lastIndex = _bcIndex;

      TR_ASSERT(!isGenerated(_bcIndex), "Walker error");
      setIsGenerated(_bcIndex);

      uint8_t opcode = _code[_bcIndex];

      TR::TreeTop *traceStop  = _block->getExit();
      TR::TreeTop *traceStart = traceStop->getPrevTreeTop();
      if (comp()->getOption(TR_TraceILGen))
         traceMsg(comp(), "%4x: %s\n", _bcIndex, ((TR_J9VM *)fej9())->getByteCodeName(opcode));

      _bc = convertOpCodeToByteCodeEnum(opcode);
      stashArgumentsForOSR(_bc);
      switch (_bc)
         {
         case J9BCinvokeinterface2:
         case J9BCnop:        _bcIndex += 1; break;

         case J9BCaconstnull: loadConstant(TR::aconst, (void *)0);  _bcIndex += 1; break;
         case J9BCiconstm1:   loadConstant(TR::iconst, -1); _bcIndex += 1; break;

         case J9BCiconst0: loadConstant(TR::iconst, 0); _bcIndex += 1; break;
         case J9BCiconst1: loadConstant(TR::iconst, 1); _bcIndex += 1; break;
         case J9BCiconst2: loadConstant(TR::iconst, 2); _bcIndex += 1; break;
         case J9BCiconst3: loadConstant(TR::iconst, 3); _bcIndex += 1; break;
         case J9BCiconst4: loadConstant(TR::iconst, 4); _bcIndex += 1; break;
         case J9BCiconst5: loadConstant(TR::iconst, 5); _bcIndex += 1; break;

         case J9BClconst0: loadConstant(TR::lconst, (int64_t)0); _bcIndex += 1; break;
         case J9BClconst1: loadConstant(TR::lconst, (int64_t)1); _bcIndex += 1; break;

         case J9BCfconst0: loadConstant(TR::fconst, 0.0f); _bcIndex += 1; break;
         case J9BCfconst1: loadConstant(TR::fconst, 1.0f); _bcIndex += 1; break;
         case J9BCfconst2: loadConstant(TR::fconst, 2.0f); _bcIndex += 1; break;

         case J9BCdconst0: loadConstant(TR::dconst, 0.0); _bcIndex += 1; break;
         case J9BCdconst1: loadConstant(TR::dconst, 1.0); _bcIndex += 1; break;

         case J9BCldc:     loadFromCP(TR::NoType, nextByte());   _bcIndex += 2; break;
         case J9BCldcw:    loadFromCP(TR::NoType, next2Bytes()); _bcIndex += 3; break;
         case J9BCldc2lw:  loadFromCP(TR::Int64,  next2Bytes()); _bcIndex += 3; break;
         case J9BCldc2dw:  loadFromCP(TR::Double, next2Bytes()); _bcIndex += 3; break;

         case J9BCiload0: loadAuto(TR::Int32, 0); _bcIndex += 1; break;
         case J9BCiload1: loadAuto(TR::Int32, 1); _bcIndex += 1; break;
         case J9BCiload2: loadAuto(TR::Int32, 2); _bcIndex += 1; break;
         case J9BCiload3: loadAuto(TR::Int32, 3); _bcIndex += 1; break;

         case J9BClload0: loadAuto(TR::Int64, 0); _bcIndex += 1; break;
         case J9BClload1: loadAuto(TR::Int64, 1); _bcIndex += 1; break;
         case J9BClload2: loadAuto(TR::Int64, 2); _bcIndex += 1; break;
         case J9BClload3: loadAuto(TR::Int64, 3); _bcIndex += 1; break;

         case J9BCfload0: loadAuto(TR::Float, 0); _bcIndex += 1; break;
         case J9BCfload1: loadAuto(TR::Float, 1); _bcIndex += 1; break;
         case J9BCfload2: loadAuto(TR::Float, 2); _bcIndex += 1; break;
         case J9BCfload3: loadAuto(TR::Float, 3); _bcIndex += 1; break;

         case J9BCdload0: loadAuto(TR::Double, 0); _bcIndex += 1; break;
         case J9BCdload1: loadAuto(TR::Double, 1); _bcIndex += 1; break;
         case J9BCdload2: loadAuto(TR::Double, 2); _bcIndex += 1; break;
         case J9BCdload3: loadAuto(TR::Double, 3); _bcIndex += 1; break;

         case J9BCaload0: loadAuto(TR::Address, 0); _bcIndex += 1; break;
         case J9BCaload1: loadAuto(TR::Address, 1); _bcIndex += 1; break;
         case J9BCaload2: loadAuto(TR::Address, 2); _bcIndex += 1; break;
         case J9BCaload3: loadAuto(TR::Address, 3); _bcIndex += 1; break;

         case J9BCiaload: loadArrayElement(TR::Int32);                              _bcIndex += 1; break;
         case J9BClaload: loadArrayElement(TR::Int64);                              _bcIndex += 1; break;
         case J9BCfaload: loadArrayElement(TR::Float);                              _bcIndex += 1; break;
         case J9BCdaload: loadArrayElement(TR::Double);                             _bcIndex += 1; break;
         case J9BCaaload: loadArrayElement(TR::Address);                            _bcIndex += 1; break;
         case J9BCbaload: loadArrayElement(TR::Int8);             genUnary(TR::b2i); _bcIndex += 1; break;
         case J9BCcaload: loadArrayElement(TR::Int16, TR::cloadi); genUnary(TR::su2i); _bcIndex += 1; break;
         case J9BCsaload: loadArrayElement(TR::Int16);            genUnary(TR::s2i); _bcIndex += 1; break;

         case J9BCiloadw: loadAuto(TR::Int32,  next2Bytes()); _bcIndex += 3; break;
         case J9BClloadw: loadAuto(TR::Int64,  next2Bytes()); _bcIndex += 3; break;
         case J9BCfloadw: loadAuto(TR::Float,   next2Bytes()); _bcIndex += 3; break;
         case J9BCdloadw: loadAuto(TR::Double,  next2Bytes()); _bcIndex += 3; break;
         case J9BCaloadw: loadAuto(TR::Address, next2Bytes()); _bcIndex += 3; break;

         case J9BCbipush: loadConstant(TR::iconst, nextByteSigned()); _bcIndex += 2; break;
         case J9BCsipush: loadConstant(TR::iconst, next2BytesSigned()); _bcIndex += 3; break;

         case J9BCiload:  loadAuto(TR::Int32,    nextByte()); _bcIndex += 2; break;
         case J9BClload:  loadAuto(TR::Int64,    nextByte()); _bcIndex += 2; break;
         case J9BCfload:  loadAuto(TR::Float,    nextByte()); _bcIndex += 2; break;
         case J9BCdload:  loadAuto(TR::Double,   nextByte()); _bcIndex += 2; break;
         case J9BCaload:  loadAuto(TR::Address,  nextByte()); _bcIndex += 2; break;

         case J9BCistore: storeAuto(TR::Int32,   nextByte()); _bcIndex += 2; break;
         case J9BClstore: storeAuto(TR::Int64,   nextByte()); _bcIndex += 2; break;
         case J9BCfstore: storeAuto(TR::Float,   nextByte()); _bcIndex += 2; break;
         case J9BCdstore: storeAuto(TR::Double,  nextByte()); _bcIndex += 2; break;
         case J9BCastore: storeAuto(TR::Address, nextByte()); _bcIndex += 2; break;

         case J9BCistore0: storeAuto(TR::Int32, 0); _bcIndex += 1; break;
         case J9BCistore1: storeAuto(TR::Int32, 1); _bcIndex += 1; break;
         case J9BCistore2: storeAuto(TR::Int32, 2); _bcIndex += 1; break;
         case J9BCistore3: storeAuto(TR::Int32, 3); _bcIndex += 1; break;

         case J9BClstore0: storeAuto(TR::Int64, 0); _bcIndex += 1; break;
         case J9BClstore1: storeAuto(TR::Int64, 1); _bcIndex += 1; break;
         case J9BClstore2: storeAuto(TR::Int64, 2); _bcIndex += 1; break;
         case J9BClstore3: storeAuto(TR::Int64, 3); _bcIndex += 1; break;

         case J9BCfstore0: storeAuto(TR::Float, 0); _bcIndex += 1; break;
         case J9BCfstore1: storeAuto(TR::Float, 1); _bcIndex += 1; break;
         case J9BCfstore2: storeAuto(TR::Float, 2); _bcIndex += 1; break;
         case J9BCfstore3: storeAuto(TR::Float, 3); _bcIndex += 1; break;

         case J9BCdstore0: storeAuto(TR::Double, 0); _bcIndex += 1; break;
         case J9BCdstore1: storeAuto(TR::Double, 1); _bcIndex += 1; break;
         case J9BCdstore2: storeAuto(TR::Double, 2); _bcIndex += 1; break;
         case J9BCdstore3: storeAuto(TR::Double, 3); _bcIndex += 1; break;

         case J9BCastore0: storeAuto(TR::Address, 0); _bcIndex += 1; break;
         case J9BCastore1: storeAuto(TR::Address, 1); _bcIndex += 1; break;
         case J9BCastore2: storeAuto(TR::Address, 2); _bcIndex += 1; break;
         case J9BCastore3: storeAuto(TR::Address, 3); _bcIndex += 1; break;

         case J9BCiastore:                   storeArrayElement(TR::Int32);            _bcIndex += 1; break;
         case J9BClastore:                   storeArrayElement(TR::Int64);            _bcIndex += 1; break;
         case J9BCfastore:                   storeArrayElement(TR::Float);            _bcIndex += 1; break;
         case J9BCdastore:                   storeArrayElement(TR::Double);           _bcIndex += 1; break;
         case J9BCaastore:                   storeArrayElement(TR::Address);          _bcIndex += 1; break;
         case J9BCbastore: genUnary(TR::i2b); storeArrayElement(TR::Int8);             _bcIndex += 1; break;
         case J9BCcastore: genUnary(TR::i2s); storeArrayElement(TR::Int16, TR::cstorei);_bcIndex += 1; break;
         case J9BCsastore: genUnary(TR::i2s); storeArrayElement(TR::Int16);            _bcIndex += 1; break;

         case J9BCistorew: storeAuto(TR::Int32,  next2Bytes()); _bcIndex += 3; break;
         case J9BClstorew: storeAuto(TR::Int64,  next2Bytes()); _bcIndex += 3; break;
         case J9BCfstorew: storeAuto(TR::Float,   next2Bytes()); _bcIndex += 3; break;
         case J9BCdstorew: storeAuto(TR::Double,  next2Bytes()); _bcIndex += 3; break;
         case J9BCastorew: storeAuto(TR::Address, next2Bytes()); _bcIndex += 3; break;

         case J9BCpop:    eat1();   _bcIndex += 1; break;
         case J9BCpop2:   eat2();   _bcIndex += 1; break;
         case J9BCdup:    dup();    _bcIndex += 1; break;
         case J9BCdup2:   dup2();   _bcIndex += 1; break;
         case J9BCdupx1:  dupx1();  _bcIndex += 1; break;
         case J9BCdup2x1: dup2x1(); _bcIndex += 1; break;
         case J9BCdupx2:  dupx2();  _bcIndex += 1; break;
         case J9BCdup2x2: dup2x2(); _bcIndex += 1; break;
         case J9BCswap:   swap();   _bcIndex += 1; break;

         case J9BCiadd:  genBinary(TR::iadd); _bcIndex += 1; break;
         case J9BCladd:  genBinary(TR::ladd); _bcIndex += 1; break;
         case J9BCfadd:  genBinary(TR::fadd); _bcIndex += 1; break;
         case J9BCdadd:  genBinary(TR::dadd); _bcIndex += 1; break;

         case J9BCisub:  genBinary(TR::isub); _bcIndex += 1; break;
         case J9BClsub:  genBinary(TR::lsub); _bcIndex += 1; break;
         case J9BCfsub:  genBinary(TR::fsub); _bcIndex += 1; break;
         case J9BCdsub:  genBinary(TR::dsub); _bcIndex += 1; break;

         case J9BCimul:  genBinary(TR::imul); _bcIndex += 1; break;
         case J9BClmul:  genBinary(TR::lmul); _bcIndex += 1; break;
         case J9BCfmul:  genBinary(TR::fmul); _bcIndex += 1; break;
         case J9BCdmul:  genBinary(TR::dmul); _bcIndex += 1; break;

         case J9BCidiv:  genIDiv(); _bcIndex += 1; break;
         case J9BCldiv:  genLDiv(); _bcIndex += 1; break;

         case J9BCirem:  genIRem(); _bcIndex += 1; break;
         case J9BClrem:  genLRem(); _bcIndex += 1; break;

         case J9BCfdiv:  genBinary(TR::fdiv); _bcIndex += 1; break;
         case J9BCddiv:  genBinary(TR::ddiv); _bcIndex += 1; break;
         case J9BCfrem:  genBinary(TR::frem); _bcIndex += 1; break;
         case J9BCdrem:  genBinary(TR::drem); _bcIndex += 1; break;

         case J9BCineg:  genUnary(TR::ineg); _bcIndex += 1; break;
         case J9BClneg:  genUnary(TR::lneg); _bcIndex += 1; break;
         case J9BCfneg:  genUnary(TR::fneg); _bcIndex += 1; break;
         case J9BCdneg:  genUnary(TR::dneg); _bcIndex += 1; break;

         case J9BCishl:  genBinary(TR::ishl);  _bcIndex += 1; break;
         case J9BCishr:  genBinary(TR::ishr);  _bcIndex += 1; break;
         case J9BCiushr: genBinary(TR::iushr); _bcIndex += 1; break;
         case J9BClshl:  genBinary(TR::lshl);  _bcIndex += 1; break;
         case J9BClshr:  genBinary(TR::lshr);  _bcIndex += 1; break;
         case J9BClushr: genBinary(TR::lushr); _bcIndex += 1; break;

         case J9BCiand:  genBinary(TR::iand); _bcIndex += 1; break;
         case J9BCior:   genBinary(TR::ior);  _bcIndex += 1; break;
         case J9BCixor:  genBinary(TR::ixor); _bcIndex += 1; break;
         case J9BCland:  genBinary(TR::land); _bcIndex += 1; break;
         case J9BClor:   genBinary(TR::lor);  _bcIndex += 1; break;
         case J9BClxor:  genBinary(TR::lxor); _bcIndex += 1; break;

         case J9BCi2l:   genUnary(TR::i2l); _bcIndex += 1; break;
         case J9BCi2f:   genUnary(TR::i2f); _bcIndex += 1; break;
         case J9BCi2d:   genUnary(TR::i2d); _bcIndex += 1; break;

         case J9BCl2i:   genUnary(TR::l2i); _bcIndex += 1; break;
         case J9BCl2f:   genUnary(TR::l2f); _bcIndex += 1; break;
         case J9BCl2d:   genUnary(TR::l2d); _bcIndex += 1; break;
         case J9BCf2i:   genUnary(TR::f2i); _bcIndex += 1; break;

         case J9BCf2d:   genUnary(TR::f2d); _bcIndex += 1; break;
         case J9BCd2i:   genUnary(TR::d2i); _bcIndex += 1; break;
         case J9BCd2f:   genUnary(TR::d2f); _bcIndex += 1; break;
         case J9BCf2l:   genUnary(TR::f2l); _bcIndex += 1; break;
         case J9BCd2l:   genUnary(TR::d2l); _bcIndex += 1; break;

         case J9BCi2b:   genUnary(TR::i2b); genUnary(TR::b2i); _bcIndex += 1; break;
         case J9BCi2c:   genUnary(TR::i2s); genUnary(TR::su2i); _bcIndex += 1; break;
         case J9BCi2s:   genUnary(TR::i2s); genUnary(TR::s2i); _bcIndex += 1; break;

         case J9BCinvokevirtual:        genInvokeVirtual(next2Bytes());        _bcIndex += 3; break;
         case J9BCinvokespecial:        genInvokeSpecial(next2Bytes());        _bcIndex += 3; break;
         case J9BCinvokestatic:         genInvokeStatic(next2Bytes());         _bcIndex += 3; break;
         case J9BCinvokeinterface:      genInvokeInterface(next2Bytes());      _bcIndex += 3; break;
         case J9BCinvokedynamic:
            if (comp()->getOption(TR_EnableOSR) && !comp()->isPeekingMethod() && !comp()->getOption(TR_FullSpeedDebug))
               _methodSymbol->setCannotAttemptOSR(_bcIndex);
            genInvokeDynamic(next2Bytes());
            _bcIndex += 3; break; // Could eventually need next3bytes
         case J9BCinvokehandle:
            if (comp()->getOption(TR_EnableOSR) && !comp()->isPeekingMethod() && !comp()->getOption(TR_FullSpeedDebug))
               _methodSymbol->setCannotAttemptOSR(_bcIndex);
            genInvokeHandle(next2Bytes());
            _bcIndex += 3; break;
         case J9BCinvokehandlegeneric:
            if (comp()->getOption(TR_EnableOSR) && !comp()->isPeekingMethod() && !comp()->getOption(TR_FullSpeedDebug))
               _methodSymbol->setCannotAttemptOSR(_bcIndex);
            genInvokeHandleGeneric(next2Bytes());
            _bcIndex += 3; break;
         case J9BCinvokespecialsplit:   genInvokeSpecial(next2Bytes() | J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG);   _bcIndex += 3; break;
         case J9BCinvokestaticsplit:    genInvokeStatic(next2Bytes() | J9_STATIC_SPLIT_TABLE_INDEX_FLAG);     _bcIndex += 3; break;

         case J9BCifeq:      loadConstant(TR::iconst, 0); _bcIndex = genIf(TR::ificmpeq, true); break;
         case J9BCifne:      loadConstant(TR::iconst, 0); _bcIndex = genIf(TR::ificmpne, true); break;
         case J9BCiflt:      loadConstant(TR::iconst, 0); _bcIndex = genIf(TR::ificmplt, true); break;
         case J9BCifge:      loadConstant(TR::iconst, 0); _bcIndex = genIf(TR::ificmpge, true); break;
         case J9BCifgt:      loadConstant(TR::iconst, 0); _bcIndex = genIf(TR::ificmpgt, true); break;
         case J9BCifle:      loadConstant(TR::iconst, 0); _bcIndex = genIf(TR::ificmple, true); break;
         case J9BCifnull:
             if (TR::Compiler->target.is64Bit())
                loadConstant(TR::aconst, (int64_t)0);
             else
                loadConstant(TR::aconst, (int32_t)0);

             _bcIndex = genIf(TR::ifacmpeq, true);
             break;

         case J9BCifnonnull:
             if (TR::Compiler->target.is64Bit())
                loadConstant(TR::aconst, (int64_t)0);
             else
                loadConstant(TR::aconst, (int32_t)0);

             _bcIndex = genIf(TR::ifacmpne, true);
             break;

         case J9BCificmpeq: _bcIndex = genIf(TR::ificmpeq); break;
         case J9BCificmpne: _bcIndex = genIf(TR::ificmpne); break;
         case J9BCificmplt: _bcIndex = genIf(TR::ificmplt); break;
         case J9BCificmpge: _bcIndex = genIf(TR::ificmpge); break;
         case J9BCificmpgt: _bcIndex = genIf(TR::ificmpgt); break;
         case J9BCificmple: _bcIndex = genIf(TR::ificmple); break;
         case J9BCifacmpeq: _bcIndex = genIf(TR::ifacmpeq); break;
         case J9BCifacmpne: _bcIndex = genIf(TR::ifacmpne); break;

         case J9BClcmp:  _bcIndex = cmp(TR::lcmp,  _lcmpOps,  lastIndex); break;
         case J9BCfcmpl: _bcIndex = cmp(TR::fcmpl, _fcmplOps, lastIndex); break;
         case J9BCfcmpg: _bcIndex = cmp(TR::fcmpg, _fcmpgOps, lastIndex); break;
         case J9BCdcmpl: _bcIndex = cmp(TR::dcmpl, _dcmplOps, lastIndex); break;
         case J9BCdcmpg: _bcIndex = cmp(TR::dcmpg, _dcmpgOps, lastIndex); break;

         case J9BCtableswitch:  _bcIndex = genTableSwitch (); break;
         case J9BClookupswitch: _bcIndex = genLookupSwitch(); break;

         case J9BCgoto:  _bcIndex = genGoto(_bcIndex + next2BytesSigned());  break;
         case J9BCgotow: _bcIndex = genGoto(_bcIndex + next4BytesSigned()); break;

         case J9BCmonitorenter: genMonitorEnter(); _bcIndex += 1; break;
         case J9BCmonitorexit:  genMonitorExit(false);  _bcIndex += 1; break;

         case J9BCathrow:       _bcIndex = genAThrow(); break;
         case J9BCarraylength:  genArrayLength();  _bcIndex += 1; break;

         case J9BCgetstatic:  loadStatic(next2Bytes());    _bcIndex += 3; break;
         case J9BCgetfield:   loadInstance(next2Bytes());  _bcIndex += 3; break;
         case J9BCputstatic:  storeStatic(next2Bytes());   _bcIndex += 3; break;
         case J9BCputfield:   storeInstance(next2Bytes()); _bcIndex += 3; break;

         case J9BCcheckcast:  genCheckCast(next2Bytes());  _bcIndex += 3; break;
         case J9BCinstanceof: genInstanceof(next2Bytes()); _bcIndex += 3; break;

         case J9BCnew:            genNew(next2Bytes());                               _bcIndex += 3; break;
         case J9BCnewarray:       genNewArray(nextByte());                            _bcIndex += 2; break;
         case J9BCanewarray:      genANewArray(next2Bytes());                         _bcIndex += 3; break;
         case J9BCmultianewarray: genMultiANewArray(next2Bytes(), _code[_bcIndex+3]); _bcIndex += 4; break;

         case J9BCiinc:  genInc();     _bcIndex += 3; break;
         case J9BCiincw: genIncLong(); _bcIndex += 5; break;

         case J9BCwide:
            {
            int32_t wopcode = _code[++_bcIndex];
            TR_J9ByteCode wbc = convertOpCodeToByteCodeEnum(wopcode);
            if (_bcIndex > lastIndex)
               lastIndex = _bcIndex;
            if (wbc == J9BCiinc)
               { genIncLong(); _bcIndex += 5; break; }

            switch (wbc)
               {
               case J9BCiload:  loadAuto(TR::Int32,    next2Bytes()); break;
               case J9BClload:  loadAuto(TR::Int64,    next2Bytes()); break;
               case J9BCfload:  loadAuto(TR::Float,    next2Bytes()); break;
               case J9BCdload:  loadAuto(TR::Double,   next2Bytes()); break;
               case J9BCaload:  loadAuto(TR::Address,  next2Bytes()); break;

               case J9BCistore: storeAuto(TR::Int32,   next2Bytes()); break;
               case J9BClstore: storeAuto(TR::Int64,   next2Bytes()); break;
               case J9BCfstore: storeAuto(TR::Float,   next2Bytes()); break;
               case J9BCdstore: storeAuto(TR::Double,  next2Bytes()); break;
               case J9BCastore: storeAuto(TR::Address, next2Bytes()); break;
               default: break;
               }
            _bcIndex += 3;
            break;
            }

         case J9BCgenericReturn:
            _bcIndex = genReturn(method()->returnOpCode(), method()->isSynchronized());
            break;

         case J9BCunknown:
            fej9()->unknownByteCode(comp(), opcode);
            break;

         default:
         	break;
         }

      if (comp()->getOption(TR_TraceILGen))
         {
         TR::StackMemoryRegion stackMemoryRegion(*comp()->trMemory());

         TR_BitVector beforeTreesInserted(comp()->getNodeCount(), trMemory(), stackAlloc, growable);
         TR_BitVector afterTreesInserted (comp()->getNodeCount(), trMemory(), stackAlloc, growable);

         comp()->getDebug()->saveNodeChecklist(beforeTreesInserted);
         printTrees(comp(), traceStart->getNextTreeTop(), traceStop, "trees inserted");
         comp()->getDebug()->saveNodeChecklist(afterTreesInserted);

         // Commoning in the "stack after" printout should match that in the
         // "trees inserted" printout.
         //
         // NOTE: this is disabled because it prints (potentially large) trees
         // twice for no particular benefit.  Instead, we have opted to have
         // the "stack after" printout appear to be "commoned" with the "trees
         // inserted" printout.  This might cause some minor confusion when the
         // "stack after" section contains nodes with refcount=1 that appear to
         // be commoned, but this overall clarity seems to favour the terser format.
         //
         //comp()->getDebug()->restoreNodeChecklist(beforeTreesInserted);
         printStack(comp(), _stack, "stack after");

         traceMsg(comp(), "  ============================================================\n");

         // Commoning from now on will reflect trees already inserted, not
         // those that happened to appear on the stack.  (The desirability
         // of the resulting verbosity is debatable.)
         //
         comp()->getDebug()->restoreNodeChecklist(afterTreesInserted);
         }
      }

   if( _blocksToInline) // partial inlining - only generate goto if its in the list of blocks.
      {
      if(_blocksToInline->getHighestBCIndex() > lastIndex)
         {
         lastIndex = _blocksToInline->getHighestBCIndex();
         //printf("Walker: setting lastIndex to %d\n",lastIndex);
         }
      if(_blocksToInline->getLowestBCIndex() < firstIndex)
         {
         firstIndex = _blocksToInline->getLowestBCIndex();
         //printf("Walker: setting firstIndex to %d\n",firstIndex);
         }
      }

   // join the basic blocks
   //
   TR::Block * lastBlock = NULL, * nextBlock, * block = blocks(firstIndex);
   if (firstIndex == 0)
      cfg()->addEdge(cfg()->getStart(), block);
   else
      prevBlock->getExit()->join(block->getEntry());
   for (i = firstIndex; block; lastBlock = block, block = nextBlock)
      {
      while (block->getNextBlock())
         {
         TR_ASSERT( block->isAdded(), "Block should have already been added\n" );
         block = block->getNextBlock();
         }

      block->setIsAdded();

      for (nextBlock = 0; !nextBlock && ++i <= lastIndex; )
         if (isGenerated(i) && blocks(i) && !blocks(i)->isAdded())
            nextBlock = blocks(i);

      // If an exception range ends with an if and the fall through is
      // in the main-line code then we have to generate a fall through block
      // which contains a goto to jump back to the main-line code.
      //
      TR::Node * lastRealNode = block->getLastRealTreeTop()->getNode();
      if (!nextBlock && lastRealNode->getOpCode().isIf())
         {
         nextBlock = TR::Block::createEmptyBlock(comp());
         i = lastIndex;
         if(_blocksToInline)
            {
            if(!blocks(i+3))
               {
               TR_ASSERT(_blocksToInline->hasGeneratedRestartTree(),"Fall Thru Block doesn't exist and we don't have a restart tree.\n");
               nextBlock->append(
                  TR::TreeTop::create(comp(),
                     TR::Node::create(lastRealNode, TR::Goto, 0, _blocksToInline->getGeneratedRestartTree())));
               }
            else
               {
               nextBlock->append(
                  TR::TreeTop::create(comp(),
                     TR::Node::create(lastRealNode, TR::Goto, 0, blocks(i + 3)->getEntry())));
               }
            }
         else
            {
            TR_ASSERT(blocks(i + 3), "can't find the fall thru block");
            nextBlock->append(
               TR::TreeTop::create(comp(),
                  TR::Node::create(lastRealNode, TR::Goto, 0, blocks(i + 3)->getEntry())));
            }
         }
      block->getExit()->getNode()->copyByteCodeInfo(lastRealNode);
      cfg()->insertBefore(block, nextBlock);
      }

   if(_blocksToInline && _blocksToInline->hasGeneratedRestartTree())
      {
      _blocksToInline->getGeneratedRestartTree()->getEnclosingBlock()->setIsCold();
      _blocksToInline->getGeneratedRestartTree()->getEnclosingBlock()->setFrequency(1);
      }

   return lastBlock;
   }

//----------------------------------------------
// walker helper routines
//----------------------------------------------

int32_t
TR_J9ByteCodeIlGenerator::cmp(TR::ILOpCodes cmpOpcode, TR::ILOpCodes * combinedOpcodes, int32_t & lastIndex)
   {
   int32_t nextBCIndex = _bcIndex + 1;
   uint8_t nextOpcode = _code[nextBCIndex];

   // can't generate the async check here if someone is jumping to it
   //
   if (convertOpCodeToByteCodeEnum(nextOpcode) == J9BCasyncCheck && blocks(nextBCIndex) == 0)
      {
      genAsyncCheck();
      nextBCIndex = ++_bcIndex + 1;
      nextOpcode = _code[nextBCIndex];
      if (_bcIndex > lastIndex)
         lastIndex = _bcIndex;
      }

   TR::ILOpCodes combinedOpcode;
   switch (convertOpCodeToByteCodeEnum(nextOpcode))
      {
      case J9BCifeq: combinedOpcode = combinedOpcodes[0]; break;
      case J9BCifne: combinedOpcode = combinedOpcodes[1]; break;
      case J9BCiflt: combinedOpcode = combinedOpcodes[2]; break;
      case J9BCifge: combinedOpcode = combinedOpcodes[3]; break;
      case J9BCifgt: combinedOpcode = combinedOpcodes[4]; break;
      case J9BCifle: combinedOpcode = combinedOpcodes[5]; break;
      default:     combinedOpcode = TR::BadILOp;         break;
      }

   // don't combine the opcodes if someone is jumping to the if
   //
   if (combinedOpcode != TR::BadILOp && blocks(nextBCIndex) == 0)
      return cmpFollowedByIf(nextOpcode, combinedOpcode, lastIndex);

   genBinary(cmpOpcode);
   genUnary(TR::b2i);
   return _bcIndex + 1;
   }

int32_t
TR_J9ByteCodeIlGenerator::cmpFollowedByIf(uint8_t ifOpcode, TR::ILOpCodes combinedOpcode, int32_t & lastIndex)
   {
   if (++_bcIndex > lastIndex)
      lastIndex = _bcIndex;
   return genIf(combinedOpcode);
   }

//----------------------------------------------
// gen helper routines
//----------------------------------------------

TR::SymbolReference *
TR_J9ByteCodeIlGenerator::placeholderWithDummySignature()
   {
   // Note: signatures should always be correct.  Only call this to pass the
   // result to something like genNodeAndPopChildren which will expand the
   // signature properly.
   if (comp()->getOption(TR_TraceMethodIndex))
      traceMsg(comp(), "placeholderWithDummySignature using owning symbol M%p _methodSymbol: M%p\n", comp()->getJittedMethodSymbol(), _methodSymbol);

   // Note that we use comp()->getJittedMethodSymbol() instead of _methodSymbol here.
   // The caller doesn't matter for this special method, and there's no need to make
   // potentially hundreds of symbols for the same method.
   //
   return comp()->getSymRefTab()->methodSymRefFromName(comp()->getJittedMethodSymbol(), JSR292_ILGenMacros, JSR292_placeholder, JSR292_placeholderSig, TR::MethodSymbol::Static);
   }

TR::SymbolReference *
TR_J9ByteCodeIlGenerator::placeholderWithSignature(char *prefix, int prefixLength, char *middle, int middleLength, char *suffix, int suffixLength)
   {
   return symRefWithArtificialSignature(placeholderWithDummySignature(),
      ".#.#.#",
      prefix, prefixLength,
      middle, middleLength,
      suffix, suffixLength);
   }

TR::SymbolReference *
TR_J9ByteCodeIlGenerator::symRefWithArtificialSignature(TR::SymbolReference *original, char *effectiveSigFormat, ...)
   {
   TR::StackMemoryRegion stackMemoryRegion(*comp()->trMemory());

   va_list args;
   va_start(args, effectiveSigFormat);
   char *effectiveSig = vartificialSignature(stackAlloc, effectiveSigFormat, args);
   va_end(args);

   TR::SymbolReference *result = comp()->getSymRefTab()->methodSymRefWithSignature(original, effectiveSig, strlen(effectiveSig));

   return result;
   }

static int32_t processArtificialSignature(char *result, char *format, va_list args)
   {
   int32_t resultLength = 0;
   char *cur = result;
   for (int32_t i = 0; format[i]; i++)
      {
      int32_t length=-1;
      char   *startChar=NULL;
      if (format[i] == '.') // period is the ONLY character (besides null) that can never appear in a method signature
         {
         // Formatting code
         switch (format[++i])
            {
            case '@': // insert a single given arg out of a given signature
               {
               char *sig = va_arg(args, char*);
               int   n   = va_arg(args, int);
               startChar = nthSignatureArgument(n, sig+1);
               length    = nextSignatureArgument(startChar) - startChar;
               break;
               }
            case '-': // insert a given range of args out of a given signature
               {
               char *sig    = va_arg(args, char*);
               int   firstN = va_arg(args, int);
               int   lastN  = va_arg(args, int);
               if (lastN >= firstN)
                  {
                  startChar = nthSignatureArgument(firstN,  sig+1);
                  length    = nthSignatureArgument(lastN+1, sig+1) - startChar;
                  }
               else
                  {
                  startChar = "";
                  length    = 0;
                  }
               break;
               }
            case '*': // insert args from a given one onward, out of a given signature
               {
               char *sig    = va_arg(args, char*);
               int   firstN = va_arg(args, int);
               startChar = nthSignatureArgument(firstN, sig+1);
               length    = strchr(startChar, ')') - startChar;
               break;
               }
            case '$': // insert return type from a given signature
               {
               char *sig = va_arg(args, char*);
               startChar = strchr(sig, ')') + 1;
               length    = nextSignatureArgument(startChar) - startChar;
               break;
               }
            case '?': // insert a given null-terminated string
               {
               startChar = va_arg(args, char*);
               length    = strlen(startChar);
               break;
               }
            case '#': // insert a given number of characters out of a given string
               {
               startChar = va_arg(args, char*);
               length    = va_arg(args, int);
               break;
               }
            default: // literal character
               TR_ASSERT(0, "Unexpected artificial signature formatting character '%c'", format[i]);

               // If we reach this point, either TR has a bug, or we have actually somehow
               // encountered a signature that legitimately had a period in it.  In the latter
               // case, proceed on the assumption that the period was a literal character;
               // if TR has a bug, we will very likely crash soon enough anyway.
               //
               startChar = format+i-1; // back up to the period
               length = 2;
               break;
            }
         }
      else
         {
         // Literal character
         startChar = format+i;
         length = 1;
         }

      TR_ASSERT(length >= 0, "assertion failure");
      TR_ASSERT(startChar != NULL, "assertion failure");

      resultLength += length;
      if (result)
         cur += sprintf(cur, "%.*s", length, startChar);

      }

   return resultLength;
   }

char *TR_J9ByteCodeIlGenerator::artificialSignature(TR_AllocationKind allocKind, char *format, ...)
   {
   va_list args;
   va_start(args, format);
   char *result = vartificialSignature(allocKind, format, args);
   va_end(args);
   return result;
   }

char *TR_J9ByteCodeIlGenerator::vartificialSignature(TR_AllocationKind allocKind, char *format, va_list args)
   {
   // Compute size
   //
   va_list argsCopy;
   va_copy(argsCopy, args);
   int32_t resultLength = processArtificialSignature(NULL, format, argsCopy);
   va_copy_end(argsCopy);

   // Produce formatted signature
   //
   char *result = (char*)trMemory()->allocateMemory(resultLength+1, allocKind);
   processArtificialSignature(result, format, args);
   return result;
   }

void
TR_J9ByteCodeIlGenerator::genArgPlaceholderCall()
   {
   // Create argument load nodes
   //
   int32_t numNodesGenerated = 0;
   ListIterator<TR::ParameterSymbol> i(&_methodSymbol->getParameterList());
   for (TR::ParameterSymbol *parm = i.getFirst(); parm; parm = i.getNext())
      {
      if (parm->getSlot() >= _argPlaceholderSlot)
         {
         // What a convoluted way to get a symref from a symbol...
         TR::SymbolReference *symRef = _methodSymbol->getParmSymRef(parm->getSlot());
         push(TR::Node::createLoad(symRef));
         numNodesGenerated++;
         }
      }

   // Create placeholder call with the proper signature
   //
   char *callerSignature = _methodSymbol->getResolvedMethod()->signatureChars();
   char *callerExpandedArgsStart = callerSignature + _argPlaceholderSignatureOffset;
   int32_t lengthOfExpandedArgs = strcspn(callerExpandedArgsStart, ")");
   TR::SymbolReference *placeholderSymRef = placeholderWithSignature("(", 1, callerExpandedArgsStart, lengthOfExpandedArgs, ")I", 2);
   push(genNodeAndPopChildren(TR::icall, numNodesGenerated, placeholderSymRef));
   }

static bool isPlaceholderCall(TR::Node *node)
   {
   if (node->getOpCode().isCall() && node->getSymbol()->getResolvedMethodSymbol())
      return node->getSymbol()->castToResolvedMethodSymbol()->getMandatoryRecognizedMethod() == TR::java_lang_invoke_ILGenMacros_placeholder;
   else
      return false;
   }

int32_t
TR_J9ByteCodeIlGenerator::expandPlaceholderCall()
   {
   TR::Node *placeholder = pop();
   TR_ASSERT(isPlaceholderCall(placeholder), "expandPlaceholderCall expects placeholder call on top of stack");
   if (comp()->getOption(TR_TraceILGen))
      traceMsg(comp(), "  Expanding placeholder call %s\n", comp()->getDebug()->getName(placeholder->getSymbolReference()));
   for (int i = 0; i < placeholder->getNumChildren(); i++)
      push(placeholder->getAndDecChild(i));
   return placeholder->getNumChildren()-1; // there was already 1 for the placeholder itself
   }

TR::SymbolReference *
TR_J9ByteCodeIlGenerator::expandPlaceholderSignature(TR::SymbolReference *symRef, int32_t numArgs)
   {
   return expandPlaceholderSignature(symRef, numArgs, numArgs);
   }

TR::SymbolReference *
TR_J9ByteCodeIlGenerator::expandPlaceholderSignature(TR::SymbolReference *symRef, int32_t numArgs, int32_t firstArgStackDepth)
   {
   if (!symRef->getSymbol()->getResolvedMethodSymbol())
      return symRef;

   TR_ResolvedMethod *originalMethod = symRef->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();

   int32_t firstArgStackOffset = _stack->size() - firstArgStackDepth;
   int32_t currentArgSignatureOffset = 1; // skip parenthesis
   for (int32_t childIndex = originalMethod->isStatic()? 0 : 1; childIndex < numArgs; childIndex++)
      {
      int32_t explicitArgIndex = childIndex - originalMethod->isStatic()? 0 : 1;
      TR_ResolvedMethod *symRefMethod = symRef->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();
      char   *signatureChars  = symRefMethod->signatureChars();
      int nextArgSignatureOffset = nextSignatureArgument(signatureChars + currentArgSignatureOffset) - signatureChars;

      TR_ASSERT(signatureChars[currentArgSignatureOffset] != ')', "expandPlaceholderSignature must not walk past the end of the argument portion of the signature");

      TR::Node *child = _stack->element(firstArgStackOffset + childIndex);
      if (isPlaceholderCall(child))
         {
         // Replace the current argument's signature chars with the chars between the parentheses of the child's signature
         int32_t signatureLength = symRefMethod->signatureLength();
         char *childSignatureChars = child->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->signatureChars();
         int32_t numCharsInserted = strcspn(childSignatureChars+1, ")");
         symRef = symRefWithArtificialSignature(symRef,
            ".#.#.#", // TODO:JSR292: It may be possible to simplify this logic drastically with a more clever format
            signatureChars,         currentArgSignatureOffset,
            childSignatureChars+1,  numCharsInserted,
            signatureChars+nextArgSignatureOffset, signatureLength - nextArgSignatureOffset);
// This doesn't work...
//            "(-**)$",
//            symRefMethod->signatureChars(), 0, explicitArgIndex-1, // first n-1 args
//            childSignatureChars, 0,                                // child's args in place of current arg
//            symRefMethod->signatureChars(), explicitArgIndex+1,    // last m args
//            symRefMethod->signatureChars()                         // return type
//            );
         currentArgSignatureOffset += numCharsInserted;
         }
      else
         {
         currentArgSignatureOffset = nextArgSignatureOffset;
         }
      }
   return symRef;
   }

int32_t
TR_J9ByteCodeIlGenerator::numPlaceholderCalls(int32_t depthLimit)
   {
   int32_t result = 0;
   for (int32_t i = 0; i < depthLimit; i++)
      if (isPlaceholderCall(_stack->element(_stack->topIndex()-i)))
         result++;
   return result;
   }

int32_t
TR_J9ByteCodeIlGenerator::expandPlaceholderCalls(int32_t depthLimit)
   {
   if (depthLimit <= 0)
      {
      return 0;
      }
   else
      {
      TR::Node *topNode = pop();
      int32_t numAdditionalNodes = expandPlaceholderCalls(depthLimit-1);
      push(topNode);
      if (isPlaceholderCall(_stack->element(_stack->topIndex())))
         numAdditionalNodes += expandPlaceholderCall();
      return numAdditionalNodes;
      }
   }

bool TR_J9ByteCodeIlGenerator::isFinalFieldFromSuperClasses(J9Class *methodClass, int32_t subClassFieldOffset)
   {
   J9Class *superClass = (J9Class *) fej9()->getSuperClass( (TR_OpaqueClassBlock *) methodClass);

   if (superClass == NULL)
      return false;

   int32_t superClassInstanceSize = TR::Compiler->cls.classInstanceSize((TR_OpaqueClassBlock *) superClass);
   if (subClassFieldOffset < superClassInstanceSize)
      return true;
   else
      return false;
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genNodeAndPopChildren(TR::ILOpCodes opcode, int32_t numChildren, TR::SymbolReference * symRef, int32_t firstIndex, int32_t lastIndex)
   {
   if (numPlaceholderCalls(lastIndex-firstIndex+1) > 0) // JSR292
      {
      symRef = expandPlaceholderSignature(symRef, lastIndex-firstIndex+1);
      int32_t numAdditionalNodes = expandPlaceholderCalls(lastIndex-firstIndex+1);
      numChildren += numAdditionalNodes;
      lastIndex   += numAdditionalNodes;

      if (comp()->getOption(TR_TraceILGen))
         {
         traceMsg(comp(), "  Expanded placeholder(s) needing %d additional nodes -- resulting symref: %s\n", numAdditionalNodes, comp()->getDebug()->getName(symRef));

         TR::StackMemoryRegion stackMemoryRegion(*comp()->trMemory());

         TR_BitVector before(comp()->getNodeCount(), trMemory(), stackAlloc, growable);
         printStack(comp(), _stack, "stack after expandPlaceholderCalls");
         comp()->getDebug()->restoreNodeChecklist(before);
         }
      }

   TR::Node * node = TR::Node::createWithSymRef(opcode, numChildren, symRef);
   for (int32_t i = lastIndex; i >= firstIndex; --i)
      node->setAndIncChild(i, pop());
   return node;
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genNodeAndPopChildren(TR::ILOpCodes opcode, int32_t numChildren, TR::SymbolReference * symRef, int32_t firstIndex)
   {
   return genNodeAndPopChildren(opcode, numChildren, symRef, firstIndex, numChildren-1);
   }

void
TR_J9ByteCodeIlGenerator::genUnary(TR::ILOpCodes unaryOp, bool isForArrayAccess)
   {
   TR::Node *node = TR::Node::create(unaryOp, 1, pop());
   if(isForArrayAccess)
      {
      traceMsg(comp(), "setting i2l node %p n%dn nonegative because it's for array access\n");
      node->setIsNonNegative(true);
      }
   push(node);
   }

bool
TR_J9ByteCodeIlGenerator::swapChildren(TR::ILOpCodes nodeop, TR::Node * firstChild)
   {
   return TR::ILOpCode(nodeop).getOpCodeForSwapChildren() &&
          (firstChild->getOpCode().isLoadConst() ||
           (firstChild->getOpCode().isLoadVar() && firstChild->getSymbol()->isConst()));
   }

void
TR_J9ByteCodeIlGenerator::genBinary(TR::ILOpCodes nodeop, int32_t numChildren)
   {
   TR::Node * second = pop();
   TR::Node * first = pop();
   if (swapChildren(nodeop, first))
      push(TR::Node::create(TR::ILOpCode(nodeop).getOpCodeForSwapChildren(), numChildren, second, first));
   else
      push(TR::Node::create(nodeop, numChildren, first, second));
   }

TR::TreeTop *
TR_J9ByteCodeIlGenerator::genTreeTop(TR::Node * n)
   {
   if (!n->getOpCode().isTreeTop())
      n = TR::Node::create(TR::treetop, 1, n);

   //In involuntaryOSR, exception points are OSR points but we don't need to
   //handle pending pushes for them because the operand stack is always empty at catch.
   bool isExceptionOnlyPoint = comp()->getOSRMode() == TR::involuntaryOSR && !n->canGCandReturn() && n->canGCandExcept();
   if (comp()->getOption(TR_TraceOSR))
      traceMsg(comp(), "skip saving PPS for exceptionOnlyPoints %d node n%dn\n", isExceptionOnlyPoint, n->getGlobalIndex());

   // It is not necessary to save the stack under OSR if the bytecode index or caller has been marked as cannotAttemptOSR
   bool cannotAttemptOSR = comp()->getOption(TR_EnableOSR) && !comp()->isPeekingMethod() &&
      (_methodSymbol->cannotAttemptOSRAt(n->getByteCodeInfo(), NULL, comp()) || _cannotAttemptOSR);
   if (comp()->getOption(TR_TraceOSR) && cannotAttemptOSR)
      traceMsg(comp(), "skip saving PPS for cannotAttemptOSR at %d:%d node n%dn\n", n->getByteCodeIndex(), n->getByteCodeInfo().getCallerIndex(), n->getGlobalIndex());

   if (!comp()->isPeekingMethod() && comp()->isPotentialOSRPoint(n) && !isExceptionOnlyPoint && !cannotAttemptOSR)
      {
      static const char *OSRPPSThreshold;
      static int32_t osrPPSThresh = (OSRPPSThreshold = feGetEnv("TR_OSRPPSThreshold")) ? atoi(OSRPPSThreshold) : 0;
      static const char *OSRTotalPPSThreshold;
      static int32_t osrTotalPPSThresh = (OSRTotalPPSThreshold = feGetEnv("TR_OSRTotalPPSThreshold")) ? atoi(OSRTotalPPSThreshold) : 0;

      static const char *OSRPPSThresholdOutsideLoops;
      static int32_t osrPPSThreshOutsideLoops = (OSRPPSThresholdOutsideLoops = feGetEnv("TR_OSRPPSThresholdOutsideLoops")) ? atoi(OSRPPSThresholdOutsideLoops) : 0;
      static const char *OSRTotalPPSThresholdOutsideLoops;
      static int32_t osrTotalPPSThreshOutsideLoops = (OSRTotalPPSThresholdOutsideLoops = feGetEnv("TR_OSRTotalPPSThresholdOutsideLoops")) ? atoi(OSRTotalPPSThresholdOutsideLoops) : 0;

      static const char *OSRLoopNestingThreshold;
      static int32_t osrLoopNestingThresh = (OSRLoopNestingThreshold = feGetEnv("TR_OSRLoopNestingThreshold")) ? atoi(OSRLoopNestingThreshold) : 1;

      static const char *OSRIndirectCallBCThreshold;
      static int32_t osrIndirectCallBCThresh = (OSRIndirectCallBCThreshold = feGetEnv("TR_OSRIndirectCallBCThreshold")) ? atoi(OSRIndirectCallBCThreshold) : 0;

      bool OSRTooExpensive = false;
      if ((n->getNumChildren() > 0) && !comp()->getOption(TR_EnableOSROnGuardFailure) && comp()->getHCRMode() != TR::osr &&

          (((comp()->getNumLoopNestingLevels() == 0) &&
            ((((int32_t) _stack->size()) > osrPPSThreshOutsideLoops) || true ||
             ((((int32_t) _stack->size() + (int32_t) comp()->getNumLivePendingPushSlots())) > osrTotalPPSThreshOutsideLoops))) ||

           ((comp()->getNumLoopNestingLevels() >= osrLoopNestingThresh) &&
            ((((int32_t) _stack->size()) > osrPPSThresh) ||
             ((((int32_t) _stack->size() + (int32_t) comp()->getNumLivePendingPushSlots())) > osrTotalPPSThresh))) ||

           (n->getFirstChild()->getOpCode().isCallIndirect() &&
            (n->getFirstChild()->getSymbolReference()->isUnresolved() || true || // turned OSR off for indirect calls for 727 due to failed guards being seen
             n->getFirstChild()->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->virtualMethodIsOverridden() ||
             comp()->getPersistentInfo()->isClassLoadingPhase() ||
             (comp()->getOption(TR_EnableHCR) &&
              (!n->getFirstChild()->getSecondChild()->getOpCode().isLoadVarDirect() || !n->getFirstChild()->getSecondChild()->getSymbol()->isParm())) ||
             (n->getFirstChild()->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->maxBytecodeIndex() > osrIndirectCallBCThresh)))
           ))
        {
        OSRTooExpensive = true;
        if (n->getFirstChild()->getOpCode().isCall() && !comp()->getOption(TR_FullSpeedDebug) && comp()->getOption(TR_EnableOSR))
           {
           if (comp()->getOption(TR_TraceOSR))
              traceMsg(comp(), "Skipping OSR due to cost at bci %d.%d\n", comp()->getCurrentInlinedSiteIndex(), n->getFirstChild()->getByteCodeIndex());
           _methodSymbol->setCannotAttemptOSR(n->getFirstChild()->getByteCodeIndex());
           }
        }

      if ((comp()->getOption(TR_MimicInterpreterFrameShape) ||
            comp()->getOption(TR_FullSpeedDebug) || 
            comp()->getHCRMode() == TR::osr ||
            ((n->getNumChildren() > 0) && n->getFirstChild()->getOpCode().isCall() && !OSRTooExpensive && comp()->getOption(TR_EnableOSR))) 
            && !comp()->isPeekingMethod())
         {
         if (comp()->isOSRTransitionTarget(TR::postExecutionOSR))
            {
            // When transitioning to a call bytecode, the VM requires that its
            // arguments be provided as children to the induce call
            // As the transition will occur at the next bytecode in post
            // execution, ensure the arguments for the next bytecode are
            // stashed
            //
            _couldOSRAtNextBC = true;
            if ((n->getOpCode().isCheck() || n->getOpCodeValue() == TR::treetop) && n->getFirstChild()->getOpCode().isCall())
               {
               // Calls are only OSR points for their first evaluation. Other references to the call,
               // that are also anchored under checks or treetops, are not OSR points. This can be
               // identified based on a node checklist, as the reference count may be unreliable.
               //
               if (!_processedOSRNodes->contains(n->getFirstChild()))
                  {
                  _processedOSRNodes->add(n->getFirstChild());

                  // The current stack should contain the current method's state prior to the call,
                  // with the call's arguments popped off. If this call is later inlined, it may
                  // contain an OSR transition. Therefore, it is necessary to save the current
                  // method's state now, so that it is available when the transition is made.
                  handlePendingPushSaveSideEffects(n);
                  saveStack(-1);
                  stashPendingPushLivenessForOSR();
                  }
               else if (comp()->getOption(TR_TraceOSR))
                  {
                  traceMsg(comp(), "Skipping OSR stack state for repeated call n%dn in treetop n%dn\n", n->getFirstChild()->getGlobalIndex(), n->getGlobalIndex());
                  }

               return _block->append(TR::TreeTop::create(comp(), n));
               }
            else
               {
               // Save the stack after the treetop has been created and appended
               handlePendingPushSaveSideEffects(n);
               TR::TreeTop *toReturn = _block->append(TR::TreeTop::create(comp(), n));
               saveStack(-1, !comp()->pendingPushLivenessDuringIlgen());
               stashPendingPushLivenessForOSR(comp()->getOSRInductionOffset(n));

               return toReturn;
               }
            }
         else
            {
            handlePendingPushSaveSideEffects(n);

            // saveStack(-1) actually stores out the current PPS to the PPS save
            // region so that the FSD stackwalker can copy it to the VM frame as
            // part of decompilation
            //
            saveStack(-1);

            // Currently, livenss will not run under involuntary OSR, so stashing
            // the pending push liveness is only necessary in the voluntary case
            if (comp()->getOSRMode() == TR::voluntaryOSR)
               stashPendingPushLivenessForOSR();
            }
         }
      }
   return _block->append(TR::TreeTop::create(comp(), n));
   }

void
TR_J9ByteCodeIlGenerator::removeIfNotOnStack(TR::Node *n)
   {
   startCountingStackRefs();
   n->incReferenceCount();
   n->recursivelyDecReferenceCount();
   stopCountingStackRefs();
   }

void
TR_J9ByteCodeIlGenerator::popAndDiscard(int n)
   {
   TR_ASSERT(n >= 0, "Number of nodes to pop and discard must be non-negative");
   startCountingStackRefs();
   for (int i = 0; i < n; i++)
      pop()->recursivelyDecReferenceCount();
   stopCountingStackRefs();
   }

void
TR_J9ByteCodeIlGenerator::discardEntireStack()
   {
   startCountingStackRefs();
   while (!_stack->isEmpty())
      pop()->recursivelyDecReferenceCount();
   // stack is empty, no need to stopCountingStackRefs()
   }

void
TR_J9ByteCodeIlGenerator::startCountingStackRefs()
   {
   for (int i = 0; i < _stack->size(); i++)
      _stack->element(i)->incReferenceCount();
   }

void
TR_J9ByteCodeIlGenerator::stopCountingStackRefs()
   {
   for (int i = 0; i < _stack->size(); i++)
      _stack->element(i)->decReferenceCount();
   }

// saveStack is called to save the operand stack, normally when it might
// be live across BB (as in the call from genTarget). In addition, in
// FSD mode, saveStack is sometimes called with no target block (with
// targetIndex < 0), which means we're saving pending pop slots (PPS) at a
// decompilation point (GC return point?) for FSD.
//
// In any case we store each operand stack/PPS (Pending Pop Stack) slot
// to a region of the frame used exclusively for saving and restoring
// PPS slots.
//
// if there is no PPS (Pending Pop Stack) for the target bytecode
// (Which must be the first in the target bb) we create one which is
// subsequenly initialized with the current PPS.  This is just a
// lazy initialization.
//
// When a bb is translated _stackTemps initially is a copy of the
// upwardly exposed operand stack to the current bb(?)  Then, as slot i
// is saved by this method _stackTemps[i] is set to the node
// representing the value stored. This allows us to trivially avoid
// redundantly saving the same slot over and over. (Presumably this is
// primarily useful at decompilation points in FSD mode though it would
// also avoid saving slots that flow through a block undisturbed.)
//
// The code below appears to save all PPS deeper than _stackTemp but
// only those shallower which are not the same as the corresponding
// slots in _stackTemp. Presumably this is because it is not necessary
// to save a PPS that already must have been saved on entry to the
// current block, ie: when (i <= _stackTemps.topIndex())
//
// Finally, when we are initializing a target block, we add the live
// slots to the target bb's initial stack_.  This is okay even if there
// are multiple successors because the JVM spec requires that the
// incoming PPS to each successor bb must be identical.
//
// int parm targetIndex indicates bytecode index of first op in the target bb.
// bool parm anchorLoads will ensure any pending push temps that have not
//   been modified since the last call to saveStack will have the corresponding
//   pending push slot anchored
//
void
TR_J9ByteCodeIlGenerator::saveStack(int32_t targetIndex)
   {
   saveStack(targetIndex, false);
   }

void
TR_J9ByteCodeIlGenerator::saveStack(int32_t targetIndex, bool anchorLoads)
   {
   if (_stack->isEmpty())
      return;

   static const char *disallowOSRPPS2 = feGetEnv("TR_DisallowOSRPPS2");
   bool loadPP = !disallowOSRPPS2 && comp()->getOption(TR_EnableOSR) && !comp()->isOSRTransitionTarget(TR::postExecutionOSR);
   bool createTargetStack = (targetIndex >= 0 && !_stacks[targetIndex]);
   if (createTargetStack)
      _stacks[targetIndex] = new (trStackMemory()) ByteCodeStack(trMemory(), std::max<uint32_t>(20, _stack->size()));

   int32_t i;
   int32_t tempIndex = 0;
   for (i = 0; i < _stack->size(); ++i)
      {
      if (!isPlaceholderCall(_stack->element(i)))
         {
         if (_stackTemps.topIndex() < tempIndex || _stackTemps[tempIndex] != _stack->element(i))
            handlePendingPushSaveSideEffects(_stack->element(i), tempIndex);
         tempIndex++;
         }
      else
         {
         for (int32_t j = 0; j < _stack->element(i)->getNumChildren(); ++j)
            {
            TR::Node* child = _stack->element(i)->getChild(j);
            if (_stackTemps.topIndex() < tempIndex || _stackTemps[tempIndex] != child)
               handlePendingPushSaveSideEffects(child, tempIndex);
            tempIndex++;
            }
         }
      }

   // There are three different indices here:
   //    tempIndex: Index into stackTemps, which must take into account the additional children under placeholder calls
   //    slot: The pending push slot, which must take into account the number of slots required by each node on the stack
   //    i: Index into _stack
   //
   int32_t slot = 0;
   tempIndex = 0;
   for (i = 0; i < _stack->size(); ++i)
      {
      TR::Node * n = _stack->element(i);

      if (!isPlaceholderCall(n))
         {
         TR::SymbolReference * symRef = symRefTab()->findOrCreatePendingPushTemporary(_methodSymbol, slot, getDataType(n));
         if (_stackTemps.topIndex() < tempIndex || _stackTemps[tempIndex] != n)
            {
            genTreeTop(TR::Node::createStore(symRef, n));

            // Under preExecutionOSR, the stack elements are saved to pending push temps and then the corresponding
            // slots on the stack are replaced by loads of these temps. This ensures the pending push temp appears live
            // across any OSR points, due to the use of the load when it is popped off the stack.
            //
            if (loadPP)
               {
               TR::Node *load = TR::Node::createLoad(symRef);
               (*_stack)[i] = load;
               _stackTemps[tempIndex] = load;
               }
            else
               _stackTemps[tempIndex] = n;
            }

         // Under postExecutionOSR, the stack elements are saved to pending push slots but the corresponding slots on the stack
         // are not modified. Instead, calls to saveStack where anchorLoads is true will achieve the same effect for liveness
         // by generating anchored loads of any pending push slots that have not been modified. There will be several superfluous
         // loads, but they can be cleaned up in genILOpts once the OSR liveness analysis is complete.
         //
         else if (anchorLoads
             && comp()->getOption(TR_EnableOSR)
             && comp()->isOSRTransitionTarget(TR::postExecutionOSR)
             && _stackTemps[tempIndex] == n
             && !(n->getOpCode().hasSymbolReference() && n->getSymbolReference() == symRef))
            genTreeTop(TR::Node::createLoad(symRef));

         // this arranges that PPS i is reloaded on entry to the successor
         // basic block, which is the one starting at bytecode index targetIndex
         //
         if (createTargetStack)
            (*_stacks[targetIndex])[i] = TR::Node::createLoad(symRef);

         slot += n->getNumberOfSlots();
         tempIndex++;
         }
      else
         {
         // A placeholder call has several children which represent the true contents of the stack
         // For correctness, these children should be stored into pending push slots
         //
         for (int32_t j = 0; j < n->getNumChildren(); ++j)
            {
            TR::Node* child = n->getChild(j);
            TR::SymbolReference * symRef = symRefTab()->findOrCreatePendingPushTemporary(_methodSymbol, slot, getDataType(child));
            if (_stackTemps.topIndex() < tempIndex || _stackTemps[tempIndex] != child)
               {
               genTreeTop(TR::Node::createStore(symRef, child));

               if (loadPP)
                  {
                  TR::Node *load = TR::Node::createLoad(symRef);
                  child->recursivelyDecReferenceCount();
                  n->setAndIncChild(j, load);
                  _stackTemps[tempIndex] = load;
                  }
               else
                  _stackTemps[tempIndex] = child;
               }
            else if (anchorLoads
                && comp()->getOption(TR_EnableOSR)
                && comp()->isOSRTransitionTarget(TR::postExecutionOSR)
                && _stackTemps[tempIndex] == child
                && !(child->getOpCode().hasSymbolReference() && child->getSymbolReference() == symRef))
               genTreeTop(TR::Node::createLoad(symRef));

            slot += child->getNumberOfSlots();
            tempIndex++;
            }

         // For this to be supported, pending push loads would always have to replace the placeholder's children in
         // all conditions
         TR_ASSERT_FATAL(!createTargetStack, "Cannot store and later load placeholder calls in current implementation");
         }
      }
   }

// Bad code results if code following saveStack generated stores
// attempts to load from one of the saved PPS slots. Two obvious
// examples arise. For the first example see the comment before
// TR_J9ByteCodeIlGenerator::genIf. Second, consider a treetop preceeding a
// decompilation point. Nodes loaded from the PPS save region (i.e that
// were live on entry to the block) may be referred to by treetops on
// both sides of the decompilation point and thus may be killed by the
// stores (soon to be) generated by saveStack.
//
// In both these scenarios the problem is that saveStack assumes the
// execution mode of bytecode rather than the TR IL. The saveStack
// method simply walks the simulated operand stack and generates a
// store treetop for each Node it finds on the simulated stack. This,
// like bytecode, assumes that operations are completed at the time
// values are pushed on the simulated stack.
//
// Meanwhile, TR IL is being generated that refers to Nodes popped off
// the stack. The assumption here is that the order of operations is
// unconstrained other than by the data flow implied by the trees of
// references to nodes and by the control dependancies imposed by the
// order of treetops. The latter are implied by the semantics of each
// bytecode.
//
// The job of handlePendingPushSaveSideEffects is to add additional
// implicit control dependancies to the TR IL caused by the
// decompilation points.  All loads of the PPS save region must occur
// before decompilation points except in the specfic case when the load
// would redundantly reload a value already in the PPS.
//
// If handlePendingPushSaveSideEffects is being called on the stack
// contents in saveStack, before they are stored in to pending push temps,
// it is possible to eliminate some of the excess anchored pending push
// loads generated by this function using the stackSize parm. As
// the pending pushes are modified in ascending order, it is safe to assume
// any pending push slots equal to the current stack slot or greater have
// not yet been stored to and are, therefore, safe to load from.
//
void
TR_J9ByteCodeIlGenerator::handlePendingPushSaveSideEffects(TR::Node * n, int32_t stackSize)
   {
   if (_stack->size() == 0)
      return;

   TR::NodeChecklist checklist(comp());
   handlePendingPushSaveSideEffects(n, checklist, stackSize);
   }

void
TR_J9ByteCodeIlGenerator::handlePendingPushSaveSideEffects(TR::Node * n, TR::NodeChecklist &checklist, int32_t stackSize)
   {
   if (checklist.contains(n))
      return;
   checklist.add(n);

   // if any tree on the stack can be modified by the sideEffect then the node
   // on the stack must become a treetop before the sideEffect node is evaluated
   //
   for (int32_t i = n->getNumChildren() - 1; i >= 0; --i)
      handlePendingPushSaveSideEffects(n->getChild(i), checklist, stackSize);

   // constant pool indices less than 0 indicate loads from saved pending
   // pop slot locations.  In addition, the value is the negation of the
   // PPS slot number the load is to.
   //
   if (n->getOpCode().isLoadVarDirect() && n->getSymbolReference()->isTemporary(comp()) && n->getSymbolReference()->getCPIndex() < 0)
      {
      // loadSlot: the pending push slot being loaded from
      int32_t loadSlot = -n->getSymbolReference()->getCPIndex() - 1;
      // absStackSlot: index into stack elements, taking placeholder children into account
      int32_t absStackSlot = 0;
      // child: keep track of placeholder children if necessary
      int32_t child = -1;
      // modifiedSlot: iterate through the slots currently in use
      int32_t modifiedSlot = 0;
      // i: iterate through the stack elements
      int32_t i = 0;
      for (i = 0; i < _stack->size(); ++i)
         {
         if (!isPlaceholderCall(_stack->element(i)))
            {
            if (modifiedSlot >= loadSlot)
               {
               child = -1;
               break;
               }
            modifiedSlot += _stack->element(i)->getNumberOfSlots();
            absStackSlot++;
            }
         else
            {
            for (child = 0; child < _stack->element(i)->getNumChildren(); ++child)
               {
               if (modifiedSlot >= loadSlot)
                  break;
               modifiedSlot += _stack->element(i)->getChild(child)->getNumberOfSlots();
               absStackSlot++;
               }
            if (child < _stack->element(i)->getNumChildren())
               break;
            }
         }

      // If there is a node on the stack that has uses the same slot, it could result in the pending push temp
      // being modified when the stack is saved, so it is anchored
      //
      if (modifiedSlot == loadSlot
          && (stackSize == -1 || stackSize > absStackSlot)
          && i < _stack->size()
          && ((child >= 0 && _stack->element(i)->getChild(child) != n) || (child == -1 && _stack->element(i) != n)))
         {
         genTreeTop(n);
         }
      }
   }

/*
 * Stash the required number of arguments for the provided bytecode.
 * The current stack will be walked, determining pending push temps for
 * the required arguments. It is assumed that the arguments would have
 * already been stored to these symrefs.
 */
void
TR_J9ByteCodeIlGenerator::stashArgumentsForOSR(TR_J9ByteCode byteCode)
   {
   if (!_couldOSRAtNextBC)
      return;
   _couldOSRAtNextBC = false;

   if (comp()->isPeekingMethod()
       || !comp()->getOption(TR_EnableOSR)
       || _cannotAttemptOSR 
       || !comp()->isOSRTransitionTarget(TR::postExecutionOSR))
      return;

   // Determine the symref to extract the number of arguments required by this bytecode
   TR::SymbolReference *symRef;
   switch (byteCode)
      {
      case J9BCinvokevirtual:
         symRef = symRefTab()->findOrCreateVirtualMethodSymbol(_methodSymbol, next2Bytes());
         break;
      case J9BCinvokespecial:
         symRef = symRefTab()->findOrCreateSpecialMethodSymbol(_methodSymbol, next2Bytes());
         break;
      case J9BCinvokestatic:
         symRef = symRefTab()->findOrCreateStaticMethodSymbol(_methodSymbol, next2Bytes());
         break;
      case J9BCinvokeinterface:
         symRef = symRefTab()->findOrCreateInterfaceMethodSymbol(_methodSymbol, next2Bytes());
         break;
      case J9BCinvokeinterface2:
         symRef = symRefTab()->findOrCreateInterfaceMethodSymbol(_methodSymbol, next2Bytes(3));
         break;
      case J9BCinvokedynamic:
         symRef = symRefTab()->findOrCreateDynamicMethodSymbol(_methodSymbol, next2Bytes());
         break;
      case J9BCinvokehandle:
      case J9BCinvokehandlegeneric:
         symRef = symRefTab()->findOrCreateHandleMethodSymbol(_methodSymbol, next2Bytes());
         break;
      case J9BCinvokestaticsplit:
         symRef = symRefTab()->findOrCreateStaticMethodSymbol(_methodSymbol, next2Bytes() | J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG);
         break;
      case J9BCinvokespecialsplit:
         symRef = symRefTab()->findOrCreateSpecialMethodSymbol(_methodSymbol, next2Bytes() | J9_STATIC_SPLIT_TABLE_INDEX_FLAG);
         break;
      default:
         return;
      }

   TR::MethodSymbol *symbol = symRef->getSymbol()->castToMethodSymbol();
   int32_t numArgs = symbol->getMethod()->numberOfExplicitParameters() + (symbol->isStatic() ? 0 : 1);
   
   TR_OSRMethodData *osrMethodData =
      comp()->getOSRCompilationData()->findOrCreateOSRMethodData(comp()->getCurrentInlinedSiteIndex(), _methodSymbol);
   osrMethodData->ensureArgInfoAt(_bcIndex, numArgs);

   // Walk the stack, grabbing that last numArgs elements
   // It is necessary to walk the whole stack to determine the slot numbers
   int32_t slot = 0;
   int arg = 0;
   for (int32_t i = 0; i < _stack->size(); ++i)
      {
      TR::Node * n = _stack->element(i);
      if (_stack->size() - numArgs <= i) 
         {
         TR::SymbolReference * symRef = symRefTab()->findOrCreatePendingPushTemporary(_methodSymbol, slot, getDataType(n));
         osrMethodData->addArgInfo(_bcIndex, arg, symRef->getReferenceNumber());
         arg++;
         }
      slot += n->getNumberOfSlots(); 
      }
   }

// Under voluntary OSR, it is preferable to keep only the essential symrefs live
// for the transition. Computing the liveness can be costly however. To reduce
// this compile time overhead, pending push liveness can be solved during IlGen.
//
// This method will solve the liveness for the current bci with the offset applied
// and stash the result against the OSR method data.
void
TR_J9ByteCodeIlGenerator::stashPendingPushLivenessForOSR(int32_t offset)
   {
   if (!comp()->pendingPushLivenessDuringIlgen())
      return;

   TR_OSRMethodData *osrData = comp()->getOSRCompilationData()->findOrCreateOSRMethodData(
      comp()->getCurrentInlinedSiteIndex(), comp()->getMethodSymbol());

   // Reset any existing pending push mapping
   // This is due to the overlap with arg stashing
   TR_BitVector *livePP = osrData->getPendingPushLivenessInfo(_bcIndex + offset);
   if (livePP)
      livePP->empty();

   int32_t slot = 0;
   int arg = 0;
   for (int32_t i = 0; i < _stack->size(); ++i)
      {
      TR::Node *n = _stack->element(i);
      TR::SymbolReference *symRef = symRefTab()->findOrCreatePendingPushTemporary(_methodSymbol, slot, getDataType(n));
      if (livePP)
         livePP->set(symRef->getReferenceNumber());
      else
         {
         livePP = new (trHeapMemory()) TR_BitVector(0, trMemory(), heapAlloc);
         livePP->set(symRef->getReferenceNumber());
         osrData->addPendingPushLivenessInfo(_bcIndex + offset, livePP);
         }
      slot += n->getNumberOfSlots();
      }
   }

void
TR_J9ByteCodeIlGenerator::handleSideEffect(TR::Node * sideEffect)
   {
   // if any tree on the stack can be modified by the sideEffect then the node
   // on the stack must become a treetop before the sideEffect node is evaluated
   //
   for (int32_t i = 0; i < _stack->size(); ++i)
      {
      TR::Node * n = _stack->element(i);
      if (n->getReferenceCount() == 0 && valueMayBeModified(sideEffect, n))
         genTreeTop(n);
      }
   }

bool
TR_J9ByteCodeIlGenerator::valueMayBeModified(TR::Node * sideEffect, TR::Node * node)
   {
   if (isPlaceholderCall(node))
      return false;  // Placeholders have no side-effects

   if (node->getOpCode().hasSymbolReference() && sideEffect->mayModifyValue(node->getSymbolReference()))
      return true;

   int32_t numChilds = node->getNumChildren();
   for (int32_t i = 0; i < numChilds; ++i)
      if (valueMayBeModified(sideEffect, node->getChild(i)))
         return true;

   return false;
   }


TR::Node *
TR_J9ByteCodeIlGenerator::loadConstantValueIfPossible(TR::Node *topNode, uintptrj_t topFieldOffset,  TR::DataType type, bool isArrayLength)
   {
   TR::Node *constNode = NULL;
   TR::Node *parent = topNode;
   TR::SymbolReference *symRef = NULL;
   uintptrj_t fieldOffset = 0;
   if (topNode->getOpCode().hasSymbolReference())
      {
      symRef = topNode->getSymbolReference();
      if (symRef->getSymbol()->isShadow() &&
         symRef->getSymbol()->isFinal() &&
         !symRef->isUnresolved())
         {
         fieldOffset = symRef->getOffset();
         TR::Node *child = topNode->getFirstChild();
         if (child->getOpCode().hasSymbolReference())
            {
            topNode = child;
            symRef = child->getSymbolReference();
            }
         }
      }

   if (symRef && symRef->getSymbol()->isStatic() && !symRef->isUnresolved() && symRef->getSymbol()->isFinal() && !symRef->getSymbol()->isConstObjectRef() && _method->isSameMethod(symRef->getOwningMethod(comp())))
      {
      TR::StaticSymbol *symbol = symRef->getSymbol()->castToStaticSymbol();
      TR_J9VMBase *fej9 = (TR_J9VMBase *)fe();


      bool isResolved = !symRef->isUnresolved();
      TR_OpaqueClassBlock * classOfStatic = isResolved ? _method->classOfStatic(topNode->getSymbolReference()->getCPIndex()) : 0;
      if (classOfStatic == NULL)
         {
         int len = 0;
         char * classNameOfFieldOrStatic = NULL;
         classNameOfFieldOrStatic = symRef->getOwningMethod(comp())->classNameOfFieldOrStatic(symRef->getCPIndex(), len);
         if (classNameOfFieldOrStatic)
            {
            classNameOfFieldOrStatic=classNameToSignature(classNameOfFieldOrStatic, len, comp());
            TR_OpaqueClassBlock * curClass = fej9->getClassFromSignature(classNameOfFieldOrStatic, len, symRef->getOwningMethod(comp()));
            TR_OpaqueClassBlock * owningClass = comp()->getJittedMethodSymbol()->getResolvedMethod()->containingClass();
            if (owningClass == curClass)
               classOfStatic = curClass;
            }
         }

      bool isClassInitialized = false;
      TR_PersistentClassInfo * classInfo = _noLookahead ? 0 :
         comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(classOfStatic, comp());
      if (classInfo && classInfo->isInitialized())
         isClassInitialized = true;

      bool canOptimizeFinalStatic = false;
      if (isResolved && symbol->isFinal() && !symRef->isUnresolved() &&
          classOfStatic != comp()->getSystemClassPointer() &&
          isClassInitialized &&
          !comp()->compileRelocatableCode())
         {
       //if (symbol->getDataType() == TR::Address)
            {
            // todo: figure out why classInfo would be NULL here?
            if (!classInfo->getFieldInfo())
               performClassLookahead(classInfo);
            }

         if (classInfo->getFieldInfo() && !classInfo->cannotTrustStaticFinal())
             canOptimizeFinalStatic = true;
         }

      if (canOptimizeFinalStatic)
         {
         TR::VMAccessCriticalSection loadConstantValueCriticalSection(fej9,
                                                                       TR::VMAccessCriticalSection::tryToAcquireVMAccess,
                                                                       comp());

         if (loadConstantValueCriticalSection.hasVMAccess())
            {
            uintptrj_t objectPointer = *(uintptrj_t*)symbol->getStaticAddress();
            if (objectPointer)
               {
               switch (symbol->getDataType())
                  {
                  case TR::Address:
                     {
                     if (parent != topNode)
                        objectPointer = fej9->getReferenceFieldAt(objectPointer, fieldOffset);
                     if ((type == TR::Int32) ||
                         (type == TR::Int16) ||
                         (type == TR::Int8))
                        {
                        int32_t val;
                        if (isArrayLength)
                           val = (int32_t)(fej9->getArrayLengthInElements(objectPointer));
                        else
                           val = *(int32_t*)(objectPointer + topFieldOffset);
                        loadConstant(TR::iconst, val);
                        constNode = _stack->top();
                        }
                     else if (type == TR::Int64)
                        {
                        int64_t val;
                        if (isArrayLength)
                           val = (int64_t)(fej9->getArrayLengthInElements(objectPointer));
                        else
                           val = *(int64_t*)(objectPointer + topFieldOffset);
                        loadConstant(TR::lconst, val);
                        constNode = _stack->top();
                        }
                     break;
                     }
                  default:
                     break;
                  }
               }
            }

         } // VM Access Critical Section

      }

   return constNode;
   }

//----------------------------------------------
// gen array
//----------------------------------------------
void
TR_J9ByteCodeIlGenerator::genArrayLength()
   {
   TR::Node * node = NULL;
   TR::Node * topNode = pop();

   TR::Node *loadedConst = loadConstantValueIfPossible(topNode, fej9()->getOffsetOfContiguousArraySizeField());

   if (!loadedConst)
     {
     if ( comp()->cg()->getDisableNullCheckOfArrayLength() )
        node = TR::Node::create(TR::PassThrough, 1, topNode);
     else
        node = TR::Node::create(TR::arraylength, 1, topNode);

     genTreeTop(genNullCheck(node));

     if ( comp()->cg()->getDisableNullCheckOfArrayLength() )
        {
        node = TR::Node::create(TR::arraylength, 1, topNode);
        }

      push(node);
      }
   }

void
TR_J9ByteCodeIlGenerator::genContiguousArrayLength(int32_t width)
   {
   TR::Node * node = NULL;
   TR::Node * topNode = pop();

   TR::Node *loadedConst = loadConstantValueIfPossible(topNode, fej9()->getOffsetOfContiguousArraySizeField());

   // Discontiguous arrays still require the contiguity check and can't be folded.
   //
   if (loadedConst)
      {
      if (TR::Compiler->om.isDiscontiguousArray(loadedConst->getInt(), width))
         {
         pop();
         loadedConst = NULL;
         }
      }

   if (!loadedConst)
      {
      if ( comp()->cg()->getDisableNullCheckOfArrayLength() )
         node = TR::Node::create(TR::PassThrough, 1, topNode);
      else
         node = TR::Node::create(TR::contigarraylength, 1, topNode);

      genTreeTop(genNullCheck(node));

      if ( comp()->cg()->getDisableNullCheckOfArrayLength() )
         {
         node = TR::Node::create(TR::contigarraylength, 1, topNode);
         }

      push(node);
      }
   }

void
TR_J9ByteCodeIlGenerator::genArrayBoundsCheck(TR::Node * offset, int32_t width)
   {
   bool canSkipThisBoundCheck = false;
   bool canSkipThisNullCheck = false;
   bool canSkipArrayLengthCalc = false;
   int32_t firstDimension = -1;

   if (_classInfo)
      {
      if (!_classInfo->getFieldInfo())
         performClassLookahead(_classInfo);

      // note: findFieldInfo may change baseArray
      TR::Node * baseArray = _stack->top();
      TR_PersistentFieldInfo * fieldInfo = _classInfo->getFieldInfo() ? _classInfo->getFieldInfo()->findFieldInfo(comp(), baseArray, true) : NULL;
      if (fieldInfo)
         {
         int32_t relevantDimension = _stack->top() == baseArray ? 0 : 1;
         TR_PersistentArrayFieldInfo *arrayFieldInfo = fieldInfo ? fieldInfo->asPersistentArrayFieldInfo() : 0;
         if (arrayFieldInfo && arrayFieldInfo->isDimensionInfoValid() &&
             arrayFieldInfo->getDimensionInfo(relevantDimension) >= 0)
            {
            firstDimension = arrayFieldInfo->getDimensionInfo(relevantDimension);

            /*
             * When hybrid arraylets are used, simplification based on the arraylength can only
             * be done if the arraylength fits within a single region.  Otherwise, a spine check
             * is still required.
             */

            if (!TR::Compiler->om.useHybridArraylets() || !TR::Compiler->om.isDiscontiguousArray(firstDimension, width))
               {
               if (performTransformation(comp(), "O^O CLASS LOOKAHEAD: Can skip array length calculation for array %p based on class file examination\n", baseArray))
                  canSkipArrayLengthCalc = true;
               }

            if (performTransformation(comp(), "O^O CLASS LOOKAHEAD: Can skip null check for array %p based on class file examination\n", baseArray))
               canSkipThisNullCheck = true;

            if (offset->getOpCode().isLoadConst() &&
                offset->getDataType() == TR::Int32 &&
                offset->getInt() < firstDimension &&
                offset->getInt() >= 0 &&
                (!TR::Compiler->om.useHybridArraylets() || !TR::Compiler->om.isDiscontiguousArray(firstDimension, width)))
               {
               if (performTransformation(comp(), "O^O CLASS LOOKAHEAD: Can skip bound check for access %p using array %p which has length %d based on class file examination\n", offset, baseArray, firstDimension))
                  canSkipThisBoundCheck = true;
               }
            }
         }
      }

   if (comp()->requiresSpineChecks() || (!_methodSymbol->skipBoundChecks() && !canSkipThisBoundCheck))
      {
      TR::Node *arrayLength = 0;
      if (!canSkipArrayLengthCalc)
         {
         if (!comp()->requiresSpineChecks())
            genArrayLength();
         else
            genContiguousArrayLength(width);

         arrayLength = pop();
         if (arrayLength->getOpCode().isArrayLength())
            arrayLength->setArrayStride(width);
         }
      else
         {
         TR_J9ByteCodeIteratorWithState::pop();
         arrayLength = TR::Node::create(TR::iconst, 0, firstDimension);
         }

      if (comp()->requiresSpineChecks() && !_suppressSpineChecks)
         {
         // Create an incomplete check node that will be populated when all the children
         // are known.  It must be created here to be sure it is anchored in the right spot.
         //
         TR::Node *checkNode = TR::Node::createWithSymRef(TR::BNDCHKwithSpineCHK, 4, 2, arrayLength, offset,
            symRefTab()->findOrCreateArrayBoundsCheckSymbolRef(_methodSymbol));

         genTreeTop(checkNode);
         push(checkNode);
         swap();
         }
      else
         {
         genTreeTop(TR::Node::createWithSymRef(TR::BNDCHK, 2, 2, arrayLength, offset,
                         symRefTab()->findOrCreateArrayBoundsCheckSymbolRef(_methodSymbol)));
         }
      }
   else
      {

      offset->setIsNonNegative(true);

      if (!_methodSymbol->skipNullChecks() && !canSkipThisNullCheck)
         {
         TR::Node * node = TR::Node::create(TR::PassThrough,1,pop());
         genTreeTop(genNullCheck(node));
         }
      else
         {
         TR_J9ByteCodeIteratorWithState::pop();
         }

      // Create an incomplete check node that will be populated when all the children
      // are known.  It must be created here to be sure it is anchored in the right spot.
      //
      if (comp()->requiresSpineChecks() && !_suppressSpineChecks)
         {
         TR::Node *spineChk = TR::Node::create(TR::SpineCHK, 3, offset);
         genTreeTop(spineChk);
         push(spineChk);
         swap();
         }
      else
         {
         genTreeTop(TR::Node::create(TR::treetop, 1, offset));
         }
      }

   push(offset);
   }

// Helper to calculate the address of the array element in a contiguous array
// Stack: ..., array base address, array element index
// width is the width of each array element in bytes
// headerSize is the number of header bytes at the beginning of the array
void
TR_J9ByteCodeIlGenerator::calculateElementAddressInContiguousArray(int32_t width, int32_t headerSize)
   {
   const bool isForArrayAccess = true;
   int32_t shift = TR::TransformUtil::convertWidthToShift(width);
   if (shift)
      {
      loadConstant(TR::iconst, shift);
      // generate a TR::aladd instead if required
      if (TR::Compiler->target.is64Bit())
         {
         // stack is now ...index,shift<===
         TR::Node *second = pop();
         genUnary(TR::i2l, isForArrayAccess);
         push(second);
         genBinary(TR::lshl);
         }
      else
         genBinary(TR::ishl);
      }
   if (TR::Compiler->target.is64Bit())
      {
      if (headerSize > 0)
         {
         loadConstant(TR::lconst, (int64_t)headerSize);
         // shift could have been null here (if no scaling is done for the index
         // ...so check for that and introduce an i2l if required for the aladd
         // stack now is ....index,loadConst<===
         if (!shift)
            {
            TR::Node *second = pop();
            genUnary(TR::i2l, isForArrayAccess);
            push(second);
            }
         genBinary(TR::ladd);
         }
      else if ((headerSize == 0) && (!shift))
         {
         genUnary(TR::i2l, isForArrayAccess);
         }

      genBinary(TR::aladd);
      }
   else
      {
      if (headerSize > 0)
         {
         loadConstant(TR::iconst, headerSize);
         genBinary(TR::iadd);
         }
      genBinary(TR::aiadd);
      }
   }

// Helper to calculate the index of the array element in a contiguous array
// Stack: ..., offset for array element index
// width is the width of each array element in bytes
// headerSize is the number of header bytes at the beginning of the array
void
TR_J9ByteCodeIlGenerator::calculateIndexFromOffsetInContiguousArray(int32_t width, int32_t headerSize)
   {

   if (TR::Compiler->target.is64Bit())
      {
      if (headerSize > 0)
         {
         loadConstant(TR::lconst, (int64_t)headerSize);

         genBinary(TR::lsub);
         }
      }
   else
      {
      if (headerSize > 0)
         {
         loadConstant(TR::iconst, headerSize);
         genBinary(TR::isub);
         }
      }

   int32_t shift = TR::TransformUtil::convertWidthToShift(width);
   if (shift)
      {
      loadConstant(TR::iconst, shift);
      if (TR::Compiler->target.is64Bit())
         {
         genBinary(TR::lshr);
         genUnary(TR::l2i);
         }
      else
         genBinary(TR::ishr);
      }
   }


// Helper to calculate the address of the element of an array
// RTSJ: if we should be generating arraylets, access the spine
//       then the arraylet
void
TR_J9ByteCodeIlGenerator::calculateArrayElementAddress(TR::DataType dataType, bool checks)
   {

   if (comp()->getOption(TR_EnableSIMDLibrary))
       {
       if (dataType == TR::VectorInt8)
         dataType = TR::Int8;
       else if (dataType == TR::VectorInt16)
         dataType = TR::Int16;
       else if (dataType == TR::VectorInt32)
         dataType = TR::Int32;
       else if (dataType == TR::VectorInt64)
         dataType = TR::Int64;
       if (dataType == TR::VectorFloat)
         dataType = TR::Float;
       else if (dataType == TR::VectorDouble)
         dataType = TR::Double;
       }

   int32_t width = TR::Symbol::convertTypeToSize(dataType);

   // since each element of an reference array is a compressed pointer,
   // modify the width accordingly, so the stride is 4bytes instead of 8
   //
   // J9VM_GC_COMPRESSED_POINTERS
   //
   if (comp()->useCompressedPointers() && dataType == TR::Address)
      {
      width = TR::Compiler->om.sizeofReferenceField();
      }

   // Stack is now ...,aryRef,index<===
   TR::Node * index = pop();
   dup();
   dup();
   TR::Node * nodeThatWasNullChecked = pop();

   handlePendingPushSaveSideEffects(index);
   handlePendingPushSaveSideEffects(nodeThatWasNullChecked);

   if (checks)
   {
      genArrayBoundsCheck(index, width);
   }
   else
   {
      push(index);
   }

   // Stack is now ...,aryRef,index<===
   if (comp()->generateArraylets())
      {
      // shift the index on the current stack to get index into array spine
      loadConstant(TR::iconst, fej9()->getArraySpineShift(width));
      genBinary(TR::ishr);

      // now calculate address of pointer to appropriate arraylet
      int32_t arraySpineHeaderSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
      calculateElementAddressInContiguousArray(TR::Compiler->om.sizeofReferenceField(), arraySpineHeaderSize);

      // fetch the arraylet base address
      TR::Node * elementAddress = pop();
      TR::SymbolReference * elementSymRef = symRefTab()->findOrCreateArrayletShadowSymbolRef(dataType);
      TR::Node * arrayletBaseAddr = TR::Node::createWithSymRef(TR::aloadi, 1, 1, elementAddress, elementSymRef);
      if (comp()->useCompressedPointers())
         {
         TR::Node *newArrayletBaseAddr = genCompressedRefs(arrayletBaseAddr);
         if (newArrayletBaseAddr)
            arrayletBaseAddr = newArrayletBaseAddr;
         }

      push(arrayletBaseAddr);

      // top of stack is now arraylet base address

      // get the original index back, mask it to get index into arraylet
      push(index);
      loadConstant(TR::iconst, fej9()->getArrayletMask(width));
      genBinary(TR::iand);

      // figure out the address of the element we want in the arraylet
      int32_t arrayletHeaderSize = 0;
      calculateElementAddressInContiguousArray(width, arrayletHeaderSize);
      }
   else
      {
      int32_t arrayHeaderSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
      calculateElementAddressInContiguousArray(width, arrayHeaderSize);
      _stack->top()->setIsInternalPointer(true);
      }

   push(nodeThatWasNullChecked);
   }

//----------------------------------------------
// gen check
//----------------------------------------------

void
TR_J9ByteCodeIlGenerator::genAsyncCheck()
   {
   // Create an asyncCheck tree and insert it at the start of the current block
   //

   TR::Node *node = TR::Node::createWithSymRef(TR::asynccheck,0,symRefTab()->findOrCreateAsyncCheckSymbolRef(_methodSymbol));

   if (comp()->getOption(TR_EnableOSR))
      genTreeTop(node);
   else
      _block->prepend(TR::TreeTop::create(comp(), node));
   }

/**
 * Pop object and class nodes from the stack, and emit a checkcast tree.
 *
 * @return The emitted checkcast tree
 */
void
TR_J9ByteCodeIlGenerator::genCheckCast()
   {
   TR::Node *node = genNodeAndPopChildren(TR::checkcast, 2, symRefTab()->findOrCreateCheckCastSymbolRef(_methodSymbol));
   genTreeTop(node);
   push(node->getFirstChild()); // The object reference
   _methodSymbol->setHasCheckCasts(true);
   }

/**
 * Generate IL for a checkcast bytecode instruction.
 *
 * If the class specified in the bytecode is unresolved, this leaves out the
 * ResolveCHK since it has to be conditional on a non-null object.
 *
 * @param cpIndex The constant pool entry of the class given in the bytecode
 *
 * @see expandUnresolvedClassTypeTests(List<TR::TreeTop>*)
 * @see expandUnresolvedClassCheckcast(TR::TreeTop*)
 */
void
TR_J9ByteCodeIlGenerator::genCheckCast(int32_t cpIndex)
   {
   loadClassObjectForTypeTest(cpIndex, TR_DisableAOTCheckCastInlining);
   genCheckCast();
   }

void
TR_J9ByteCodeIlGenerator::genDivCheck()
   {
   if (!_methodSymbol->skipDivChecks())
      {
      if (!cg()->getSupportsDivCheck())
         _unimplementedOpcode = _code[_bcIndex];
      genTreeTop(TR::Node::createWithSymRef(TR::DIVCHK, 1, 1, _stack->top(), symRefTab()->findOrCreateDivCheckSymbolRef(_methodSymbol)));
      }
   }

// create an integer divide node like:
// div
//   load i
//   load j
//   rem
//     ==> load i
//     ==> load j
// so that if the remainder can be commoned then on most platforms the rem is free with the div calculation
//
void
TR_J9ByteCodeIlGenerator::genIDiv()
   {
   if (cg()->getSupportsIDivAndIRemWithThreeChildren())
      {
      genBinary(TR::idiv, 3);
      TR::Node * div = _stack->top();
      div->setAndIncChild(2, TR::Node::create(TR::irem, 2, div->getFirstChild(), div->getSecondChild()));
      }
   else
      genBinary(TR::idiv);

   genDivCheck();
   }

void
TR_J9ByteCodeIlGenerator::genLDiv()
   {
   if (cg()->getSupportsLDivAndLRemWithThreeChildren())
      {
      genBinary(TR::ldiv, 3);
      TR::Node * div = _stack->top();
      div->setAndIncChild(2, TR::Node::create(TR::lrem, 2, div->getFirstChild(), div->getSecondChild()));
      }
   else
      genBinary(TR::ldiv);

   genDivCheck();
   }

void
TR_J9ByteCodeIlGenerator::genIRem()
   {
   if (cg()->getSupportsIDivAndIRemWithThreeChildren())
      {
      genBinary(TR::irem, 3);
      TR::Node * rem = _stack->top();
      rem->setAndIncChild(2, TR::Node::create(TR::idiv, 2, rem->getFirstChild(), rem->getSecondChild()));
      }
   else
      genBinary(TR::irem);

   genDivCheck();
   }

void
TR_J9ByteCodeIlGenerator::genLRem()
   {
   if (cg()->getSupportsLDivAndLRemWithThreeChildren())
      {
      genBinary(TR::lrem, 3);
      TR::Node * rem = _stack->top();
      rem->setAndIncChild(2, TR::Node::create(TR::ldiv, 2, rem->getFirstChild(), rem->getSecondChild()));
      }
   else
      genBinary(TR::lrem);

   genDivCheck();
   }


/**
 * Generate IL for an instanceof bytecode instruction.
 *
 * If the class specified in the bytecode is unresolved, this anchors the
 * instanceof node, and it leaves out the ResolveCHK since that has to be
 * conditional on a non-null object.
 *
 * @param cpIndex The constant pool entry of the class given in the bytecode
 *
 * @see expandUnresolvedClassTypeTests(List<TR::TreeTop>*)
 * @see expandUnresolvedClassInstanceof(TR::TreeTop*)
 */
void
TR_J9ByteCodeIlGenerator::genInstanceof(int32_t cpIndex)
   {
   TR::SymbolReference *classSymRef = loadClassObjectForTypeTest(cpIndex, TR_DisableAOTInstanceOfInlining);
   TR::Node *node = genNodeAndPopChildren(TR::instanceof, 2, symRefTab()->findOrCreateInstanceOfSymbolRef(_methodSymbol));
   push(node);
   if (classSymRef->isUnresolved())
      {
      // Anchor to ensure sequencing for the implied (conditional) ResolveCHK.
      genTreeTop(node);
      }
   _methodSymbol->setHasInstanceOfs(true);
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genCompressedRefs(TR::Node * address, bool genTT, int32_t isLoad)
      {
   static char *pEnv = feGetEnv("TR_UseTranslateInTrees");

   TR::Node *value = address;
   if (pEnv && (isLoad < 0)) // store
      value = address->getSecondChild();
   TR::Node *newAddress = TR::Node::createCompressedRefsAnchor(value);
   //traceMsg(comp(), "compressedRefs anchor %p generated\n", newAddress);

   if (trace())
      traceMsg(comp(), "IlGenerator: Generating compressedRefs anchor [%p] for node [%p]\n", newAddress, address);

   if (!pEnv && genTT)
      {
      genTreeTop(newAddress);
      return NULL;
      }
   else
      {
      return newAddress;
      }
   }


TR::Node *
TR_J9ByteCodeIlGenerator::genNullCheck(TR::Node * first)
   {
   // TODO : Can we get rid of the "value.getClass()" nullchecks in the Java code due to this function?

   // By default, NULLCHKs will be skipped on String.value fields.  A more general mechanism
   // is needed for skipping NULL/BNDCHKs on certain recognized fields...
   //
   static char *c = feGetEnv("TR_disableSkipStringValueNULLCHK");

   if (!_methodSymbol->skipNullChecks())
      {
      TR_ASSERT(first->getNumChildren() >= 1, "no grandchild to nullcheck?");
      TR::Node *grandChild = first->getChild(0);
      if (!c && grandChild->getOpCode().hasSymbolReference() &&
               grandChild->getSymbolReference() &&
               grandChild->getSymbolReference()->getSymbol() &&
               grandChild->getSymbolReference()->getSymbol()->getRecognizedField() == TR::Symbol::Java_lang_String_value)
         {
         if (trace())
            traceMsg(comp(), "Skipping NULLCHK (node %p) on String.value field : %s -> %s\n", grandChild, comp()->signature(), _methodSymbol->signature(trMemory()));
         }
      else
         {
         return TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, first, symRefTab()->findOrCreateNullCheckSymbolRef(_methodSymbol));
         }
      }

   if (first->getOpCode().isTreeTop())
      return first;

   return TR::Node::create(TR::treetop, 1, first);
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genResolveCheck(TR::Node * first)
   {
   return TR::Node::createWithSymRef(TR::ResolveCHK, 1, 1, first, symRefTab()->findOrCreateResolveCheckSymbolRef(_methodSymbol));
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genResolveAndNullCheck(TR::Node * first)
   {
   TR_ASSERT(first->getNumChildren() >= 1, "no grandchild to nullcheck?");
   TR::Node *grandChild = first->getChild(0);
   return TR::Node::createWithSymRef(TR::ResolveAndNULLCHK, 1, 1, first, symRefTab()->findOrCreateNullCheckSymbolRef(_methodSymbol));
   }

//----------------------------------------------
// genGoto
//----------------------------------------------

int32_t
TR_J9ByteCodeIlGenerator::genGoto(int32_t target)
   {

   if( _blocksToInline) // partial inlining - only generate goto if its in the list of blocks.
      {
      bool success=false;
      ListIterator<TR_InlineBlock> blocksIt(_blocksToInline->_inlineBlocks);
      TR_InlineBlock *aBlock = NULL;
      for (aBlock = blocksIt.getCurrent() ; aBlock; aBlock = blocksIt.getNext())
         {
            if ( aBlock->_BCIndex == target)
               {
               if (_blocks[target]->getEntry()->getNode()->getByteCodeIndex()<= _block->getEntry()->getNode()->getByteCodeIndex())
                  genAsyncCheck();
               genTreeTop(TR::Node::create(TR::Goto, 0, genTarget(target)));
               success=true;
               break;
               }
         }
      TR_ASSERT(success, " Walker: genGoto for a partial inline has no target in the list of blocks to be inlined.\n");
      }
   else
      {

      if (_blocks[target]->getEntry()->getNode()->getByteCodeIndex()
          <=       _block->getEntry()->getNode()->getByteCodeIndex())
         genAsyncCheck();

      genTreeTop(TR::Node::create(TR::Goto, 0, genTarget(target)));
      }
   return findNextByteCodeToGen();
   }

//----------------------------------------------
// genIf
//----------------------------------------------

// The "if" family of bytecodes are composed of two operations: a
// compare and a conditional branch. Like bytecode, the TR IL combines
// them into one operation. The combined operation, obviously, has to
// be the last operation in the basic block.
//
// This complicates our scheme for handling PPS slots that are live
// across blocks. The saveStack method will generate stores of any PPS
// slots live on exit. However, these stores must be generated before
// the combined compare/conditional branch operation. Thus, any loads
// from the PPS save region amongst the inputs of the compare might be
// killed by stores to be generated by saveStack. The method
// handlePendingPushSaveSideEffects is called to promote any such loads
// to treetops preceding the saveStack generated stores.
//
// singleStackOp: If the op was only intended to pop one item off the
// stack, this flag should be set to true. This is necessary to maintain
// the stack representation with the VM.
//
int32_t
TR_J9ByteCodeIlGenerator::genIf(TR::ILOpCodes nodeop, bool singleStackOp)
   {
   _methodSymbol->setHasBranches(true);
   int32_t branchBC = _bcIndex + next2BytesSigned();
   int32_t fallThruBC = _bcIndex + 3;

   TR::Node * second;
   if (singleStackOp)
      second = pop();

   if (branchBC <= _bcIndex)
      genAsyncCheck();

   if (!singleStackOp)
      second = pop();
   TR::Node * first = pop();

   handlePendingPushSaveSideEffects(first);
   handlePendingPushSaveSideEffects(second);

   if( _blocksToInline)
      {
      bool genFallThru=false, genBranchBC=false;
      ListIterator<TR_InlineBlock> blocksIt(_blocksToInline->_inlineBlocks);
      TR_InlineBlock *aBlock = NULL;
      for (aBlock = blocksIt.getCurrent() ; aBlock; aBlock = blocksIt.getNext())
         {
   //      printf("\tBlock bcIndex = %d owningMethod = %p depth = %d\n",aBlock->_BCIndex,aBlock->_owningMethod,aBlock->_depth);
         if(branchBC == aBlock->_BCIndex)
            genBranchBC=true;
         if(fallThruBC == aBlock->_BCIndex)
            genFallThru=true;
         }
//      printf("genIf: genBranchBC = %d genFallThru = %d\n",genBranchBC,genFallThru);
//      fflush(stdout);

      TR::TreeTop * branchDestination = NULL;

      if(genFallThru && genBranchBC)
         {
         genTarget(fallThruBC);
//         printf("Walker: calling genTarget on branchBC\n");
         TR::TreeTop * branchDestination = genTarget(branchBC);
         if (swapChildren(nodeop, first))
            {
            TR::TreeTop *tt = genTreeTop(TR::Node::createif(TR::ILOpCode(nodeop).getOpCodeForSwapChildren(), second, first, branchDestination));
            tt->getNode()->setSwappedChildren(true);
            }
         else
            genTreeTop(TR::Node::createif(nodeop, first, second, branchDestination));

         return findNextByteCodeToGen();
         }
      else
         {
         if(genFallThru)
            {
            genTarget(fallThruBC);

            //need to create a branch destination restart
//            printf("Walker: genIf : fallThru : getCallNodeTreeTop = %p, symreftab = %p\n",_blocksToInline->getCallNodeTreeTop(),symRefTab());
      //      fflush(stdout);
            branchDestination = _blocksToInline->hasGeneratedRestartTree() ? _blocksToInline->getGeneratedRestartTree() :
                                                                             _blocksToInline->setGeneratedRestartTree(genPartialInliningCallBack(branchBC,_blocksToInline->getCallNodeTreeTop()));  //not adding it to the queue
            if(branchBC > _blocksToInline->getHighestBCIndex())
               _blocksToInline->setHighestBCIndex(branchBC);
            else if(branchBC < _blocksToInline->getLowestBCIndex())
               _blocksToInline->setLowestBCIndex(branchBC);
            //printf("Walker: genIf : fallThru : branchDestination = %p\n",branchDestination);
//            fflush(stdout);
            //_blocksToInline->getCallNodeTreeTop();
            }

         if(genBranchBC)
            {
            //printf("Walker: genIf : BranchBC : hasGeneratedRestartTree = %d getGeneratedRestartTree = %p getEnclosingBlock = %p\n",_blocksToInline->hasGeneratedRestartTree(),_blocksToInline->getGeneratedRestartTree(), _blocksToInline->getGeneratedRestartTree() ? _blocksToInline->getGeneratedRestartTree()->getEnclosingBlock() : 0);
//            fflush(stdout);
            _blocksToInline->hasGeneratedRestartTree() ? genGotoPartialInliningCallBack(fallThruBC,_blocksToInline->getGeneratedRestartTree()) :
                                                         _blocksToInline->setGeneratedRestartTree(genPartialInliningCallBack(fallThruBC,_blocksToInline->getCallNodeTreeTop()));
            if(fallThruBC > _blocksToInline->getHighestBCIndex())
               _blocksToInline->setHighestBCIndex(fallThruBC);
            else if(fallThruBC < _blocksToInline->getLowestBCIndex())
               _blocksToInline->setLowestBCIndex(fallThruBC);
            branchDestination = genTarget(branchBC);
            //printf("Walker: genIf : BranchBC : branchDestination = %p\n",branchDestination);
 //           fflush(stdout);
   //genTreeTop(TR::Node::create(TR::Goto, 0, genTarget(target)));
            }
         //generating the if statement regardless methinks
         TR_ASSERT(branchDestination, "Walker: No branchDestination in partial inlining\n");
         if (swapChildren(nodeop, first))
            {
            TR::TreeTop *tt = genTreeTop(TR::Node::createif(TR::ILOpCode(nodeop).getOpCodeForSwapChildren(), second, first, branchDestination));
            tt->getNode()->setSwappedChildren(true);
            }
         else
            genTreeTop(TR::Node::createif(nodeop, first, second, branchDestination));

         return findNextByteCodeToGen();
         }
      }
   else
      {
      genTarget(fallThruBC);
//      printf("Walker: calling genTarget on branchBC\n");
      TR::TreeTop * branchDestination = genTarget(branchBC);
      if (swapChildren(nodeop, first))
         {
         TR::TreeTop *tt = genTreeTop(TR::Node::createif(TR::ILOpCode(nodeop).getOpCodeForSwapChildren(), second, first, branchDestination));
         tt->getNode()->setSwappedChildren(true);
         }
      else
         genTreeTop(TR::Node::createif(nodeop, first, second, branchDestination));

      return findNextByteCodeToGen();
      }
   }

//----------------------------------------------
// genInc
//----------------------------------------------

void
TR_J9ByteCodeIlGenerator::genInc()
   {
   int32_t index = nextByte();
   loadAuto(TR::Int32, index);
   loadConstant(TR::iconst, nextByteSigned(2));
   genBinary(TR::iadd);
   storeAuto(TR::Int32, index);
   }

void
TR_J9ByteCodeIlGenerator::genIncLong()
   {
   int32_t index = next2Bytes();
   loadAuto(TR::Int32, index);
   loadConstant(TR::iconst, next2BytesSigned(3));
   genBinary(TR::iadd);
   storeAuto(TR::Int32, index);
   }

//----------------------------------------------
// genInvoke
//----------------------------------------------

void
TR_J9ByteCodeIlGenerator::genInvokeStatic(int32_t cpIndex)
   {
   TR::SymbolReference *methodSymRef = symRefTab()->findOrCreateStaticMethodSymbol(_methodSymbol, cpIndex);

   if (comp()->getOption(TR_TraceILGen))
      traceMsg(comp(), "  genInvokeStatic(%d) // %s\n", cpIndex, comp()->getDebug()->getName(methodSymRef));

   _staticMethodInvokeEncountered = true;

   if (runMacro(methodSymRef))
      {
      if (comp()->compileRelocatableCode())
         {
         if (comp()->getOption(TR_TraceILGen))
            traceMsg(comp(), "  ILGen macro %s not supported in AOT.  Aborting compile.\n", comp()->getDebug()->getName(methodSymRef));
         comp()->failCompilation<J9::AOTHasInvokeHandle>("An ILGen macro not supported in AOT.  Aborting compile.");
         }

      if (comp()->getOption(TR_FullSpeedDebug) && !isPeekingMethod())
         {
         if (comp()->getOption(TR_TraceILGen))
            traceMsg(comp(), "  ILGen macro %s not supported in FSD. Failing ilgen\n", comp()->getDebug()->getName(methodSymRef));
         comp()->failCompilation<J9::FSDHasInvokeHandle>("An ILGen macro not supported in FSD.  Failing ilgen.");
         }

      if (comp()->getOption(TR_TraceILGen))
         traceMsg(comp(), "  Finished macro %s\n", comp()->getDebug()->getName(methodSymRef));
      return;
      }

   TR::MethodSymbol * symbol = methodSymRef->getSymbol()->castToMethodSymbol();
   // Note that genInvokeDirect can return nodes that are not calls (for
   // recognized methods), so some caution is needed in the handling of callNode.
   TR::Node *callNode = genInvokeDirect(methodSymRef);
   if (callNode && _methodSymbol->safeToSkipChecksOnArrayCopies())
      {
      bool isCallToArrayCopy = true;
      // Defend against the case where there is no symbol reference
      if (callNode->getOpCode().hasSymbolReference() && !callNode->getSymbolReference()->isUnresolved())
         {
         TR::RecognizedMethod recognizedMethod = callNode->getSymbol()->castToResolvedMethodSymbol()->getRecognizedMethod();

         if (recognizedMethod != TR::java_lang_System_arraycopy &&
             recognizedMethod != TR::java_lang_String_compressedArrayCopy_BIBII &&
             recognizedMethod != TR::java_lang_String_compressedArrayCopy_BICII &&
             recognizedMethod != TR::java_lang_String_compressedArrayCopy_CIBII &&
             recognizedMethod != TR::java_lang_String_compressedArrayCopy_CICII &&
             recognizedMethod != TR::java_lang_String_decompressedArrayCopy_BIBII &&
             recognizedMethod != TR::java_lang_String_decompressedArrayCopy_BICII &&
             recognizedMethod != TR::java_lang_String_decompressedArrayCopy_CIBII &&
             recognizedMethod != TR::java_lang_String_decompressedArrayCopy_CICII)
            {
            isCallToArrayCopy = false;
            }
         }
      else
         isCallToArrayCopy = false;

      if (isCallToArrayCopy)
          callNode->setNodeIsRecognizedArrayCopyCall(true);
      }
   }

void
TR_J9ByteCodeIlGenerator::genInvokeSpecial(int32_t cpIndex)
   {
   TR::SymbolReference *methodSymRef = symRefTab()->findOrCreateSpecialMethodSymbol(_methodSymbol, cpIndex);

   genInvokeDirect(methodSymRef);

   // In bytecode within an interface, invokespecial instructions calling
   // anything other than <init> need to have a run-time type test to ensure
   // that the receiver is an instance of the interface. Outside of interfaces
   // this is checked during verification, but verification doesn't check
   // interface types.
   //
   // The following collects the bytecode index of each invokespecial
   // instruction that requires such a type test. The tests are inserted later
   // by expandInvokeSpecialInterface().

   const bool trace = comp()->getOption(TR_TraceILGen);
   if (skipInvokeSpecialInterfaceTypeChecks())
      {
      if (trace)
         traceMsg(comp(), "invokespecial type tests disabled by env var\n");
      return;
      }

   // Initialize _invokeSpecialInterface if necessary.
   if (!_invokeSpecialSeen)
      {
      _invokeSpecialSeen = true;

      J9Class * const textualDefClass =
         TR::Compiler->cls.convertClassOffsetToClassPtr(_method->containingClass());
      // For this purpose, an anonymous class whose host class is an interface
      // is expected to behave as though its code is contained within that
      // interface. For non-anonymous classes, hostClass is circular.
      TR_OpaqueClassBlock * const defClass =
         fej9()->convertClassPtrToClassOffset(textualDefClass->hostClass);
      if (TR::Compiler->cls.isInterfaceClass(comp(), defClass))
         _invokeSpecialInterface = defClass;

      if (trace)
         {
         int32_t len = 6;
         const char * name = "(none)";
         if (_invokeSpecialInterface != NULL)
            name = fej9()->getClassNameChars(_invokeSpecialInterface, len);
         traceMsg(comp(),
            "within interface %p %.*s for the purpose of invokespecial\n",
            _invokeSpecialInterface,
            len, name);
         }
      }

   if (_invokeSpecialInterface == NULL)
      {
      if (trace)
         traceMsg(comp(), "no invokespecial type tests in this method\n");
      return;
      }

   TR_Method *callee = methodSymRef->getSymbol()->castToMethodSymbol()->getMethod();
   if (callee->isConstructor())
      {
      if (trace)
         traceMsg(comp(), "no invokespecial type test for constructor\n");
      return;
      }

   if (callee->isFinalInObject())
      {
      if (trace)
         traceMsg(comp(), "invokespecial of final Object method is really invokevirtual\n");
      return;
      }

   const int32_t bcIndex = currentByteCodeIndex();
   if (comp()->compileRelocatableCode())
      {
      if (isOutermostMethod())
         {
         TR::DebugCounter::incStaticDebugCounter(comp(),
            TR::DebugCounter::debugCounterName(comp(),
               "ilgen.abort/aot-invokespecial-interface/root/(%s)/bc=%d",
               comp()->signature(),
               bcIndex));
         }
      else
         {
         TR::DebugCounter::incStaticDebugCounter(comp(),
            TR::DebugCounter::debugCounterName(comp(),
               "ilgen.abort/aot-invokespecial-interface/inline/(%s)/bc=%d/root=(%s)",
               _method->signature(comp()->trMemory()),
               bcIndex,
               comp()->signature()));
         }

      comp()->failCompilation<J9::AOTHasInvokeSpecialInInterface>(
         "COMPILATION_AOT_HAS_INVOKESPECIAL_IN_INTERFACE");
      }

   // Make note of this invokespecial; it needs a run-time type test against
   // _invokeSpecialInterface.
   if (_invokeSpecialInterfaceCalls == NULL) // lazily allocated
      {
      _invokeSpecialInterfaceCalls =
         new (trHeapMemory()) TR_BitVector(_maxByteCodeIndex + 1, trMemory());
      }

   _invokeSpecialInterfaceCalls->set(bcIndex);
   if (trace)
      traceMsg(comp(), "request invokespecial type test at bc index %d\n", bcIndex);
   }

void
TR_J9ByteCodeIlGenerator::genInvokeVirtual(int32_t cpIndex)
   {
   TR::SymbolReference * symRef = symRefTab()->findOrCreateVirtualMethodSymbol(_methodSymbol, cpIndex);
   TR::Symbol * sym = symRef->getSymbol();

   TR_ResolvedMethod * method = symRef->isUnresolved() ? 0 : sym->castToResolvedMethodSymbol()->getResolvedMethod();
   bool isDirectCall = (method &&
                        (sym->isFinal() ||
                         (debug("omitVirtualGuard") && !method->virtualMethodIsOverridden())));

   if (isDirectCall)
      {
      genInvokeDirect(symRef);
      }
   else
      {
      genInvokeWithVFTChild(symRef);
      _methodSymbol->setMayHaveIndirectCalls(true);
      }
   }

void
TR_J9ByteCodeIlGenerator::genInvokeInterface(int32_t cpIndex)
   {
   TR::SymbolReference * symRef = symRefTab()->findOrCreateInterfaceMethodSymbol(_methodSymbol, cpIndex);

   genInvokeWithVFTChild(symRef);
   _methodSymbol->setMayHaveIndirectCalls(true);
   }

static char *suffixedName(char *baseName, char typeSuffix, char *buf, int32_t bufSize, TR::Compilation *comp)
   {
   char *methodName = buf;
   int32_t methodNameLength = strlen(baseName) + 2;
   if (methodNameLength >= bufSize)
      methodName = (char*)comp->trMemory()->allocateStackMemory(methodNameLength+1, TR_MemoryBase::IlGenerator);
   sprintf(methodName, "%s%c", baseName, typeSuffix);
   return methodName;
   }

void
TR_J9ByteCodeIlGenerator::genInvokeDynamic(int32_t callSiteIndex)
   {
   if (comp()->compileRelocatableCode())
      {
      comp()->failCompilation<J9::AOTHasInvokeHandle>("COMPILATION_AOT_HAS_INVOKEHANDLE 0");
      }

   if (comp()->getOption(TR_FullSpeedDebug) && !isPeekingMethod())
      comp()->failCompilation<J9::FSDHasInvokeHandle>("FSD_HAS_INVOKEHANDLE 0");

   TR::SymbolReference *symRef = symRefTab()->findOrCreateDynamicMethodSymbol(_methodSymbol, callSiteIndex);

   // Compute the receiver handle
   //
   loadFromCallSiteTable(callSiteIndex);
   TR::Node *receiver = top();

   if (comp()->getOption(TR_TraceILGen))
      printStack(comp(), _stack, "(Stack after load from callsite table)");

   // If the receiver handle is resolved, we can use a more specific symref
   //
   TR_ResolvedMethod * owningMethod = _methodSymbol->getResolvedMethod();
   if (!owningMethod->isUnresolvedCallSiteTableEntry(callSiteIndex))
      {
      TR_ResolvedMethod *specimen = fej9()->createMethodHandleArchetypeSpecimen(trMemory(), (uintptrj_t*)owningMethod->callSiteTableEntryAddress(callSiteIndex), owningMethod);
      if (specimen)
         symRef = symRefTab()->findOrCreateMethodSymbol(_methodSymbol->getResolvedMethodIndex(), -1, specimen, TR::MethodSymbol::ComputedVirtual);
      }

   if (comp()->getOption(TR_TraceILGen))
      printStack(comp(), _stack, "(Stack before genInvokeHandle)");

   if (comp()->getOption(TR_EnableMHCustomizationLogicCalls))
      {
      dup();
      TR::SymbolReference *doCustomizationLogic = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_MethodHandle, "doCustomizationLogic", "()V", TR::MethodSymbol::Special);
      genInvokeDirect(doCustomizationLogic);
      }

   // Emit the call
   //
   genInvokeHandle(symRef, receiver);
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genInvokeHandle(int32_t cpIndex)
   {
   if (comp()->compileRelocatableCode())
      {
      comp()->failCompilation<J9::AOTHasInvokeHandle>("COMPILATION_AOT_HAS_INVOKEHANDLE 1");
      }

   if (comp()->getOption(TR_FullSpeedDebug) && !isPeekingMethod())
      comp()->failCompilation<J9::FSDHasInvokeHandle>("FSD_HAS_INVOKEHANDLE 1");
   // Locate the receiver handle
   //
   TR::SymbolReference * invokeExactSymRef = symRefTab()->findOrCreateHandleMethodSymbol(_methodSymbol, cpIndex);
   TR::Node *receiverHandle = getReceiverFor(invokeExactSymRef);

   // Emit the MethodType check
   //
   if (fej9()->hasMethodTypesSideTable())
      loadFromMethodTypeTable(cpIndex);
   else
      loadFromCP(TR::NoType, cpIndex);

   TR::Node *callSiteType = pop();
   genHandleTypeCheck(receiverHandle, callSiteType);

   if (comp()->getOption(TR_EnableMHCustomizationLogicCalls))
      {
      push(receiverHandle);
      TR::SymbolReference *doCustomizationLogic = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_MethodHandle, "doCustomizationLogic", "()V", TR::MethodSymbol::Special);
      genInvokeDirect(doCustomizationLogic);
      }

   // Emit the call
   //
   push(receiverHandle);
   return genInvokeHandle(invokeExactSymRef);
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genInvokeHandle(TR::SymbolReference *invokeExactSymRef, TR::Node *invokedynamicReceiver)
   {
   if (comp()->getOption(TR_TraceILGen))
      printStack(comp(), _stack, "(Stack before genInvokeHandle)");

   TR::SymbolReference *invokeExactTargetAddress = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_MethodHandle, JSR292_invokeExactTargetAddress, JSR292_invokeExactTargetAddressSig, TR::MethodSymbol::Special);
   genInvokeDirect(invokeExactTargetAddress);
   TR::Node *callNode = genInvoke(invokeExactSymRef, pop(), invokedynamicReceiver);
   _methodSymbol->setMayHaveIndirectCalls(true);
   _methodSymbol->setHasMethodHandleInvokes(true);
   if (!isPeekingMethod())
      {
      if (!comp()->getHasMethodHandleInvoke())
         {
         comp()->setHasMethodHandleInvoke(); // We only want to print this message once per compilation
         if (comp()->getOptions()->getVerboseOption(TR_VerboseMethodHandles))
            TR_VerboseLog::writeLineLocked(TR_Vlog_MH, "Jitted method contains MethodHandle invoke: %s", comp()->signature());
         }
      if (comp()->getOptions()->getVerboseOption(TR_VerboseMethodHandleDetails))
         {
         TR_Method *callee = callNode->getSymbol()->castToMethodSymbol()->getMethod();
         TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "Call to invokeExact%.*s from %s", callee->signatureLength(), callee->signatureChars(), comp()->signature());
         }
      }
   return callNode;
   }

void
TR_J9ByteCodeIlGenerator::genHandleTypeCheck()
   {
   TR::Node *expectedType = pop();
   TR::SymbolReference *getTypeSymRef = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_MethodHandle, JSR292_getType, JSR292_getTypeSig, TR::MethodSymbol::Special); // TODO:JSR292: Too bad I can't do a more general lookup and let it optimize itself.  Virtual call doesn't seem to work
   genInvokeDirect(getTypeSymRef);
   TR::Node *handleType = pop();

   genTreeTop(TR::Node::createWithSymRef(TR::ZEROCHK, 1, 1,
      TR::Node::create(TR::acmpeq, 2, expectedType, handleType),
      symRefTab()->findOrCreateMethodTypeCheckSymbolRef(_methodSymbol)));
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genInvokeHandleGeneric(int32_t cpIndex)
   {
   if (comp()->compileRelocatableCode())
      {
      comp()->failCompilation<J9::AOTHasInvokeHandle>("COMPILATION_AOT_HAS_INVOKEHANDLE 2");
      }

   if (comp()->getOption(TR_FullSpeedDebug) && !isPeekingMethod())
      comp()->failCompilation<J9::FSDHasInvokeHandle>("FSD_HAS_INVOKEHANDLE 2");

   TR::SymbolReference * invokeGenericSymRef = symRefTab()->findOrCreateHandleMethodSymbol(_methodSymbol, cpIndex);
   TR::SymbolReference * methodTypeSymRef;
   if (fej9()->hasMethodTypesSideTable())
      methodTypeSymRef = symRefTab()->findOrCreateMethodTypeTableEntrySymbol(_methodSymbol, cpIndex);
   else
      methodTypeSymRef = symRefTab()->findOrCreateMethodTypeSymbol(_methodSymbol, cpIndex);

   return genInvokeHandleGeneric(invokeGenericSymRef, methodTypeSymRef);
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genInvokeHandleGeneric(TR::SymbolReference *invokeGenericSymRef, TR::SymbolReference *methodTypeSymRef)
   {
   if (comp()->getOption(TR_TraceILGen))
      printStack(comp(), _stack, "(Stack before genInvokeHandleGeneric)");
   TR_Method *invokeGeneric = invokeGenericSymRef->getSymbol()->castToMethodSymbol()->getMethod();

   TR::Node *&receiver = _stack->element(_stack->topIndex() - invokeGeneric->numberOfExplicitParameters());
   push(receiver);
   loadSymbol(TR::aload, methodTypeSymRef);
   genTreeTop(top());
   TR::SymbolReference *typeConversion = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_MethodHandle, JSR292_asType, JSR292_asTypeSig, TR::MethodSymbol::Static);
   if (comp()->getOption(TR_TraceILGen))
      printStack(comp(), _stack, "(Stack before genTypeConversion in invokeHandleGeneric)");
   genInvokeDirect(typeConversion);
   receiver = _stack->top(); // put converted receiver where original receiver was
   if (comp()->getOption(TR_TraceILGen))
      printStack(comp(), _stack, "(Stack after genTypeConversion in invokeHandleGeneric)");

   if (comp()->getOption(TR_EnableMHCustomizationLogicCalls))
      {
      dup();
      TR::SymbolReference *doCustomizationLogic = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_MethodHandle, "doCustomizationLogic", "()V", TR::MethodSymbol::Special);
      genInvokeDirect(doCustomizationLogic);
      }

   TR::SymbolReference *invokeExactOriginal = symRefTab()->methodSymRefFromName(_methodSymbol,
      JSR292_MethodHandle, JSR292_invokeExact, JSR292_invokeExactSig, TR::MethodSymbol::ComputedVirtual, invokeGenericSymRef->getCPIndex());
   TR::SymbolReference *invokeExactSymRef = symRefTab()->methodSymRefWithSignature(
      invokeExactOriginal, invokeGeneric->signatureChars(), invokeGeneric->signatureLength());
   return genInvokeHandle(invokeExactSymRef);
   }

TR::Node*
TR_J9ByteCodeIlGenerator::genOrFindAdjunct(TR::Node* node)
   {
   TR::Node *adjunct;
   if (node->getOpCode().isLoadDirect())
      {
      // need to get adjunct symbol corresponding to this symbol, and create a load for it
      TR::SymbolReference* symRef = node->getSymbolReference();
      TR::DataType type = symRef->getSymbol()->getDataType();
      int32_t slot = symRef->getCPIndex();
      loadAuto(type, slot, true);
      adjunct = pop();
      }
   else
      {
      // expect that adjunct part is third child of node
      TR_ASSERT(node->isDualHigh() || node->isTernaryHigh(),
             "this node should be a dual or ternary, where the adjunct part of the answer is in the third child");
      adjunct = node->getChild(2);
      if (node->isTernaryHigh())
         {
         adjunct = adjunct->getFirstChild();
         }
      }
   return adjunct;
   }

#define STOPME \
   {\
   static int stopped = 0; \
   if (!stopped) \
      {\
      genDebugCmd(__FILE__, __LINE__);\
      stopped = 1;\
      }\
   }

// Definition of operation to be executed when converting the non standard lengths
struct OperationDescriptor
   {
   int32_t shiftDistance;    // distance to shift, the direction depends on load or store
   int32_t offsetAdjustment; // adjustment of the memory offset
   int32_t lengthNew;        // new length to be stored or loaded
   };
static void printOp(char *tags, struct OperationDescriptor *op)
   {
      printf("[%s] shift:%d, ajdust:%d, length:%d\n",
             tags?tags:"OP",
             op->shiftDistance, op->offsetAdjustment,
             op->lengthNew);
   }

static struct OperationDescriptor opsLength3[] =
   {
      {0,2,1},{8,0,2}, // big-endian
      {0,0,1},{8,1,2}  // little-endian
   };
static struct OperationDescriptor opsLength5[] =
   {
      {0,4,1}, {8,0,4}, // big-endian
      {0,0,1}, {8,1,4}  // little-endian
   };
static struct OperationDescriptor opsLength6[] =
   {
      {0,4,2}, {16,0,4}, // big-endian
      {0,0,2}, {16,2,4}  // little-endian
   };
static struct OperationDescriptor opsLength7[] =
   {
      {0,6,1}, {8,4,2}, {24,0,4}, // big-endian
      {0,0,1}, {8,1,2}, {24,3,4}  // little-endian
   };
static struct OperationDescriptor *opsNonStandardLengths[] =
   { 0, 0, 0, opsLength3, 0, opsLength5, opsLength6, opsLength7 };
static int32_t numOpsNonStandardLengths[] =
   { 0, 0, 0, 2, 0, 2, 2, 3};

TR::Node *
TR_J9ByteCodeIlGenerator::getReceiverFor(TR::SymbolReference *symRef)
   {
   TR_Method * method = symRef->getSymbol()->castToMethodSymbol()->getMethod();
   int32_t receiverDepth = method->numberOfExplicitParameters(); // look past all the explicit arguments
   return _stack->element(_stack->topIndex() - receiverDepth);
   }

TR::Node *
TR_J9ByteCodeIlGenerator::genInvokeWithVFTChild(TR::SymbolReference *symRef)
   {
   TR::Node *receiver = getReceiverFor(symRef);
   TR::Node *vftLoad = TR::Node::createWithSymRef(TR::aloadi, 1, 1, receiver, symRefTab()->findOrCreateVftSymbolRef());
   return genInvoke(symRef, vftLoad);
   }




TR::Node*
TR_J9ByteCodeIlGenerator::genInvoke(TR::SymbolReference * symRef, TR::Node *indirectCallFirstChild, TR::Node *invokedynamicReceiver)
   {
   TR::MethodSymbol * symbol = symRef->getSymbol()->castToMethodSymbol();
   bool isStatic     = symbol->isStatic();
   bool isDirectCall = indirectCallFirstChild == NULL;

   TR_Method * calledMethod = symbol->getMethod();
   int32_t numArgs = calledMethod->numberOfExplicitParameters() + (isStatic ? 0 : 1);
   static bool disableARMMaxMin = true; // (feGetEnv("TR_DisableARMMaxMin") != NULL);
   static bool disableARMFMaxMin = true; // (feGetEnv("TR_DisableARMFMaxMin") != NULL);

   TR::ILOpCodes opcode = TR::BadILOp;

   if (!comp()->getOption(TR_DisableMaxMinOptimization) &&
       TR::Compiler->target.cpu.isX86()) // TODO : Implement other max/min routines on X86
      {
      switch (symbol->getRecognizedMethod())
         {
         case TR::java_lang_Math_max_I:
            opcode = TR::imax;
            break;
         case TR::java_lang_Math_min_I:
            opcode = TR::imin;
            break;
         case TR::java_lang_Math_max_L:
            opcode = TR::lmax;
            break;
         case TR::java_lang_Math_min_L:
            opcode = TR::lmin;
            break;
         default:
         	break;
         }
      }
   switch (symbol->getRecognizedMethod())
      {
      case TR::java_lang_Integer_valueOf:
         // TODO: It's gross that ilgen knows what a dememoization opportunity is.  This should be refactored.
         _methodSymbol->setHasDememoizationOpportunities(true);
         break;
      default:
         break;
      }

   if (TR::Compiler->target.cpu.isPower() || (!disableARMMaxMin && TR::Compiler->target.cpu.isARM())) // TODO: implement max/min opcodes on all platforms
      {

      switch (symbol->getRecognizedMethod())
         {
         case TR::java_lang_Math_max_I:
            opcode = TR::imax;
            break;
         case TR::java_lang_Math_min_I:
            opcode = TR::imin;
            break;
         case TR::java_lang_Math_max_L:
            opcode = TR::lmax;
            break;
         case TR::java_lang_Math_min_L:
            opcode = TR::lmin;
            break;
         case TR::java_lang_Math_max_F:
            if (!disableARMFMaxMin && TR::Compiler->target.cpu.isARM())
               opcode = TR::fmax;
            break;
         case TR::java_lang_Math_min_F:
            if (!disableARMFMaxMin && TR::Compiler->target.cpu.isARM())
               opcode = TR::fmin;
            break;
         case TR::java_lang_Math_max_D:
            if (!disableARMFMaxMin && TR::Compiler->target.cpu.isARM())
               opcode = TR::dmax;
            break;
         case TR::java_lang_Math_min_D:
            if (!disableARMFMaxMin && TR::Compiler->target.cpu.isARM())
               opcode = TR::dmin;
            break;
         default:
         	break;
         }
      }

   if (opcode != TR::BadILOp)
      {
      TR::Node * node = TR::Node::create(opcode, 2);
      node->setAndIncChild(0, pop());
      node->setAndIncChild(1, pop());
      push(node);
      return node;
      }

   if (comp()->cg()->getSupportsBitOpCodes() && !comp()->getOption(TR_DisableBitOpcode))
      {
      switch (symbol->getRecognizedMethod())
         {
         case TR::java_lang_Integer_highestOneBit:
            opcode = TR::ihbit;
            break;
         case TR::java_lang_Integer_lowestOneBit:
            if(TR::Compiler->target.cpu.isX86())
               opcode = TR::ilbit;
            else
               opcode = TR::BadILOp;
            break;
         case TR::java_lang_Integer_numberOfLeadingZeros:
            opcode = TR::inolz;
            break;
         case TR::java_lang_Integer_numberOfTrailingZeros:
            opcode = TR::inotz;
            break;
         case TR::java_lang_Integer_bitCount:
            if(TR::Compiler->target.cpu.isX86() && fej9()->getX86SupportsPOPCNT())
               opcode = TR::ipopcnt;
            else if (TR::Compiler->target.cpu.hasPopulationCountInstruction())
               opcode = TR::ipopcnt;
            else
               opcode = TR::BadILOp;
            break;
         case TR::java_lang_Long_highestOneBit:
            opcode = TR::lhbit;
            break;
         case TR::java_lang_Long_lowestOneBit:
            if(TR::Compiler->target.cpu.isX86())
               opcode = TR::llbit;
            else
               opcode = TR::BadILOp;
            break;
         case TR::java_lang_Long_numberOfLeadingZeros:
            opcode = TR::lnolz;
            break;
         case TR::java_lang_Long_numberOfTrailingZeros:
            opcode = TR::lnotz;
            break;
         case TR::java_lang_Long_bitCount:
            if(TR::Compiler->target.cpu.isX86() && fej9()->getX86SupportsPOPCNT())
               opcode = TR::lpopcnt;
            else if (TR::Compiler->target.cpu.hasPopulationCountInstruction())
               opcode = TR::lpopcnt;
            else
               opcode = TR::BadILOp;
            break;
         default:
         	break;
         }
      }

   if (opcode != TR::BadILOp)
      {
      performTransformation(comp(), "O^O BIT OPCODE: convert call to method %s to bit opcode\n",calledMethod->signature(trMemory()));
      TR::Node * node = TR::Node::create(opcode, 1);
      node->setAndIncChild(0, pop());
      push(node);
      return node;
      }

#if !defined(TR_HOST_ARM)
   
   if (comp()->supportsQuadOptimization())
      {
      // Under DLT, the Quad opts don't work because the interpreted Quad
      // operations will produce Quad objects, but the jitted ones will assume
      // they have been turned into longs.

      switch (symbol->getRecognizedMethod())
         {
         case TR::com_ibm_Compiler_Internal_Quad_enableQuadOptimization:
            {
            // Quad opts have the potential to make things way worse if we
            // don't recognize the methods and special-case them here.  However,
            // if we recognize enableQuadOptimizations, we should have no trouble
            // recognizing the rest.
            //
            loadConstant(TR::iconst, 1);
            return _stack->top();
            }
         case TR::com_ibm_Compiler_Internal_Quad_mul_ll:
            {
            //      lumulh            main operator
            //        x
            //        y
            //        lmul            adjunct
            //          ==> x
            //          ==> y
            TR::Node* y = pop();
            TR::Node* x = pop();
            TR::Node* lmul   = TR::Node::create(TR::lmul, 2, x, y);
            TR::Node* lumulh = TR::Node::create(TR::lumulh, 3, x, y, lmul);
            push(lumulh);
            return lumulh;
            break;
            }
         case TR::com_ibm_Compiler_Internal_Quad_hi:
            {
            //    hi(w)
            TR::Node* wh = pop();
            push(wh);
            return wh;
            break;
            }
         case TR::com_ibm_Compiler_Internal_Quad_lo:
            {
            //   lo(w)
            TR::Node* wh = pop();
            TR::Node* wl = genOrFindAdjunct(wh);
            push(wl);
            return wl;
            break;
            }
         case TR::com_ibm_Compiler_Internal_Quad_add_ll:
            {
            // add two unsigned longs to produce a quad
            //
            //      luaddc                  main operator
            //        lconst 0
            //        lconst 0
            //        computeCC
            //          luadd               adjunct operator
            //            x
            //            y
            TR::Node* y = pop();
            TR::Node* x = pop();
            TR::Node* zero   = TR::Node::create(TR::lconst, 0, 0);
            TR::Node* luadd  = TR::Node::create(TR::luadd, 2, x, y);
            TR::Node* carry = TR::Node::create(TR::computeCC, 1, luadd);
            TR::Node* luaddc = TR::Node::create(TR::luaddc, 3, zero, zero, carry);
            push(luaddc);
            return luaddc;
            break;
            }
         case TR::com_ibm_Compiler_Internal_Quad_add_ql:
            {
            //      luaddc                  main operator
            //        wh
            //        lconst 0
            //        computeCC
            //          luadd               adjunct operator
            //            wl
            //            x
            TR::Node* x = pop();
            TR::Node* wh = pop();
            TR::Node* wl = genOrFindAdjunct(wh);
            TR::Node* zero   = TR::Node::create(TR::lconst, 0, 0);
            TR::Node* luadd  = TR::Node::create(TR::luadd, 2, wl, x);
            TR::Node* carry = TR::Node::create(TR::computeCC, 1, luadd);
            TR::Node* luaddc = TR::Node::create(TR::luaddc, 3, wh, zero, carry);
            push(luaddc);
            return luaddc;
            break;
            }
         case TR::com_ibm_Compiler_Internal_Quad_sub_ll:
            {
            // sub two unsigned longs to produce a quad
            //
            //      lusubb                  main operator
            //        lconst 0
            //        lconst 0
            //        computeCC
            //          lusub               adjunct operator
            //            x
            //            y
            TR::Node* y = pop();
            TR::Node* x = pop();
            TR::Node* zero   = TR::Node::create(TR::lconst, 0, 0);
            TR::Node* lusub  = TR::Node::create(TR::lusub, 2, x, y);
            TR::Node* borrow = TR::Node::create(TR::computeCC, 1, lusub);
            TR::Node* lusubb = TR::Node::create(TR::lusubb, 3, zero, zero, borrow);
            push(lusubb);
            return lusubb;
            break;
            }
         case TR::com_ibm_Compiler_Internal_Quad_sub_ql:
            {
            //      lusubb                  main operator
            //        wh
            //        lconst 0
            //        computeCC
            //          lusub               adjunct operator
            //            wl
            //            x
            TR::Node* x = pop();
            TR::Node* wh = pop();
            TR::Node* wl  = genOrFindAdjunct(wh);
            TR::Node* zero   = TR::Node::create(TR::lconst, 0, 0);
            TR::Node* lusub  = TR::Node::create(TR::lusub, 2, wl, x);
            TR::Node* borrow = TR::Node::create(TR::computeCC, 1, lusub);
            TR::Node* lusubb = TR::Node::create(TR::lusubb, 3, wh, zero, borrow);
            push(lusubb);
            return lusubb;
            break;
            }
         default:
         	break;
         }
      }
#endif // TR_HOST_ARM

   if (symbol->getRecognizedMethod() == TR::com_ibm_Compiler_Internal__TR_Prefetch)
      {
      TR::Node *node = NULL;

      if ((comp()->getOptLevel() < hot))
        {
        int i = 0;
        for (i=0; i<numArgs; i++)
           genTreeTop(pop());
        return node;
        }

      // Get the type of prefetch
      PrefetchType prefetchType = NoPrefetch;
      TR_Method *method = symbol->castToMethodSymbol()->getMethod();
      if (method->nameLength() == 15 && !strncmp(method->nameChars(), "_TR_Release_All", 15))
         {
         prefetchType = ReleaseAll;
         }
      else if (method->nameLength() == 17 && !strncmp(method->nameChars(), "_TR_Prefetch_Load", 17))
         {
         prefetchType = PrefetchLoad;
         }
      else if (method->nameLength() == 18 && !strncmp(method->nameChars(), "_TR_Prefetch_Store", 18))
         {
         prefetchType = PrefetchStore;
         }
      else if (method->nameLength() == 20)
         {
         if (!strncmp(method->nameChars(), "_TR_Prefetch_LoadNTA", 20))
            prefetchType = PrefetchLoadNonTemporal;
         else if (!strncmp(method->nameChars(), "_TR_Prefetch_Load_L1", 20))
            prefetchType = PrefetchLoadL1;
         else if (!strncmp(method->nameChars(), "_TR_Prefetch_Load_L2", 20))
            prefetchType = PrefetchLoadL2;
         else if (!strncmp(method->nameChars(), "_TR_Prefetch_Load_L3", 20))
            prefetchType = PrefetchLoadL3;
         }
      else if (method->nameLength() == 21)
         {
         if (!strncmp(method->nameChars(), "_TR_Prefetch_StoreNTA", 21))
            prefetchType = PrefetchStoreNonTemporal;
         else if (!strncmp(method->nameChars(), "_TR_Release_StoreOnly", 21))
            prefetchType = ReleaseStore;
         }
      else if (method->nameLength() == 29 && !strncmp(method->nameChars(), "_TR_Prefetch_StoreConditional", 29))
         {
         prefetchType = PrefetchStoreConditional;
         }

      TR::Node *n2 = pop();
      if (2 == numArgs  && n2->getOpCode().isInt())
         {
         node = TR::Node::createWithSymRef(TR::Prefetch, 4, symRef);
         TR::Node *addrNode = pop();

         // For constant 2nd arguments, we'll add it to the offset.
         if  (n2->getOpCode().isLoadConst())
            {
            // TR::Prefetch 1st child: address
            node->setAndIncChild(0, addrNode);
            // TR::Prefetch 2nd child: offset
            node->setAndIncChild(1, n2);
            }
         else
            {
            // TR::Prefetch 1st child: address
            //    aiadd
            //       addrNode
            //       offsetNode
            TR::Node * aiaddNode = TR::Node::create(TR::aiadd, 2, addrNode, n2);
            node->setAndIncChild(0, aiaddNode);

            // TR::Prefetch 2nd child: Set offset to be zero.
            TR::Node * offsetNode = TR::Node::create(TR::iconst, 0, 0);
            node->setAndIncChild(1, offsetNode);
            }

         // TR::Prefetch 3rd child : size
         TR::Node * size = TR::Node::create(TR::iconst, 0, 1);
         node->setAndIncChild(2, size);

         // TR::Prefetch 4th child : type
         TR::Node * type = TR::Node::create(TR::iconst, 0, (int32_t)prefetchType);
         node->setAndIncChild(3, type);

         genTreeTop(node);
         }
      else if (3 == numArgs || 2 == numArgs)
         {
         TR::Node * n3 = n2;   // it's already popped above
         TR::Node * n2 = pop();
         TR::Node * n1 = (numArgs == 3) ? pop() : NULL;

         if (n3->getSymbolReference() &&
             n3->getSymbolReference()->getSymbol()->isStatic() &&
             n3->getSymbolReference()->getSymbol()->castToStaticSymbol()->isConstString() &&
             n2->getSymbolReference() &&
             n2->getSymbolReference()->getSymbol()->isStatic() &&
             n2->getSymbolReference()->getSymbol()->castToStaticSymbol()->isConstString())
            {
             uintptrj_t offset = fej9()->getFieldOffset(comp(), n2->getSymbolReference(), n3->getSymbolReference() );
             if (offset)
               {
               TR::Node * n ;
               if (TR::Compiler->target.is32Bit() ||
                   (int32_t)(((uint32_t)((uint64_t)offset >> 32)) & 0xffffffff) == (uint32_t)0)
                  {
                  n = TR::Node::create(TR::iconst, 0, offset);
                  }
                  else
                  {
                  n = TR::Node::create(TR::lconst, 0, 0);
                  n->setLongInt(offset);
                  }
               if (numArgs == 3)
                  {
                  node = TR::Node::createWithSymRef(TR::Prefetch, 4, symRef);
                  node->setAndIncChild(1, n);
                  node->setAndIncChild(0, n1);
                  }
               else
                  {
                  node = TR::Node::createWithSymRef(TR::Prefetch, 4, symRef);
                  node->setAndIncChild(0, n);
                  TR::Node * offsetNode = TR::Node::create(TR::iconst, 0, 0);
                  node->setAndIncChild(1, offsetNode);
                  }

               // TR::Prefetch 3rd child : size
               TR::Node * size = TR::Node::create(TR::iconst, 0, 1);
               node->setAndIncChild(2, size);

               // TR::Prefetch 4th child : type
               TR::Node * type = TR::Node::create(TR::iconst, 0, (int32_t)prefetchType);
               node->setAndIncChild(3, type);

               genTreeTop(node);
               }
             else
               {
               // Generate treetops to retain proper reference count.
               genTreeTop(n2);
               genTreeTop(n3);
               if (n1)
                  genTreeTop(n1);

               traceMsg(comp(),"Prefetch: Unable to resolve offset.\n");
               }
            }
         }
      else
         {
         TR_ASSERT(0, "TR::Prefetch does not have two or three children");
         }

      return node;
      }

   // if optimizing for 390 or PPC hw that supports DFP instructions
   // make sure to replace 3 recognized calls with load constants

   /* Strategy on DFP HW is as follows:

      -Xdfpbd  -Xhysteresis  Meaning                DFPHWAvail     DFPPerformHys     DFPUseDFP
      ---------------------------------------------------------------------------------------
      0        0             DisableDFP+DisableHys  0              0                 n/a             (default)
      0        1             invalid                n/a            n/a               n/a             (invalid)
      1        0             EnableDFP+DisableHys   1              0                 1               (enableDFP, disableHys)
      1        1             EnableDFP+EnableHys    1              1                 n/a             (enableDFP, enableHys)

    */


   TR::Node * savedNode = NULL;
   if (_stack->topIndex() > 0)
      {
      savedNode = _stack->element(_stack->topIndex());
      }

#define DAA_PRINT(a) \
case a: \
   traceMsg(comp(), "DAA Method found: %s\n", #a); \
break


   //print out the method name and ILCode from the
   switch (symbol->getRecognizedMethod())
      {
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShort);
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShortLength);
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeInt);
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeIntLength);
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLong);
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLongLength);
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeFloat);
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeDouble);

      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShort);
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShortLength);
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readInt);
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readIntLength);
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLong);
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLongLength);
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readFloat);
      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readDouble);

      DAA_PRINT(TR::com_ibm_dataaccess_ByteArrayUtils_trailingZeros);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_JITIntrinsicsEnabled);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_ByteBuffer);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_ByteBuffer);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_ByteBuffer);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_ByteBuffer);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToLong);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToInteger);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToInteger);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertIntegerToUnicodeDecimal);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToLong);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertLongToUnicodeDecimal);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigInteger);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertBigIntegerToPackedDecimal);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigInteger);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertBigIntegerToExternalDecimal);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToBigInteger);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertBigIntegerToUnicodeDecimal);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigDecimal);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToExternalDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigDecimal);

      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToUnicodeDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToBigDecimal);

      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_addPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_subtractPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_multiplyPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_dividePackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_remainderPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_greaterThanOrEqualsPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_lessThanOrEqualsPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_equalsPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_notEqualsPackedDecimal);

      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_2bInlined2);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_2bInlined1);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal);
      DAA_PRINT(TR::com_ibm_dataaccess_PackedDecimal_movePackedDecimal);
      }

   if(symbol->getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_JITIntrinsicsEnabled)
      {
      bool isZLinux = TR::Compiler->target.isLinux() && TR::Compiler->target.cpu.isZ();
      int32_t constVal = (TR::Compiler->target.isZOS() || isZLinux) &&
              !comp()->getOption(TR_DisablePackedDecimalIntrinsics) ? 1 : 0;

      loadConstant(TR::iconst, constVal);
      return NULL;
      }

   if (!comp()->compileRelocatableCode())
      {
      bool dfpbd = comp()->getOption(TR_DisableHysteresis);
      bool nodfpbd =  comp()->getOption(TR_DisableDFP);
      bool isPOWERDFP = TR::Compiler->target.cpu.isPower() && TR::Compiler->target.cpu.supportsDecimalFloatingPoint();
      bool is390DFP =
#ifdef TR_TARGET_S390
         TR::Compiler->target.cpu.isZ() && TR::Compiler->target.cpu.getS390SupportsDFP();
#else
         false;
#endif

      int32_t constToLoad=0;

      if (isPOWERDFP || is390DFP)
         {
         if (((symbol->getRecognizedMethod() == TR::java_math_BigDecimal_DFPHWAvailable) ||
              (symbol->getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_DFPFacilityAvailable) || //disable for DAA DFP
             (!strncmp(calledMethod->signature(comp()->trMemory(), stackAlloc), BDDFPHWAVAIL, BDDFPHWAVAILLEN)))
             )
            {
            int32_t * dfpHWAvailableFlag = NULL;
            if (!fej9()->isCachedBigDecimalClassFieldAddr())
               {
               TR_OpaqueClassBlock * classBlock = fej9()->getClassFromSignature("java/math/BigDecimal", 20, comp()->getCurrentMethod());

               // BigDecimal class should be loaded if called from itself.
               // DecimalData creates a BigDecimal in its <clinit>, so the class should be loaded too.
               TR_ASSERT(NULL != classBlock, "java/lang/BigDecimal is not loaded before com/ibm/dataaccess/DecimalData.");

               if (NULL == classBlock)
                  {
                  loadConstant(TR::iconst, constToLoad);
                  return NULL;
                  }
               TR_PersistentClassInfo * classInfo = (comp()->getPersistentInfo()->getPersistentCHTable() == NULL) ?
                  NULL :
                  comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(classBlock, comp());
               if (classInfo && classInfo->isInitialized() && !comp()->compileRelocatableCode())
                  {
                  dfpHWAvailableFlag = (int32_t *)fej9()->getStaticFieldAddress(classBlock,
                                              (unsigned char *)BD_DFP_HW_AVAILABLE_FLAG, BD_DFP_HW_AVAILABLE_FLAG_LEN,
                                              (unsigned char *)BD_DFP_HW_AVAILABLE_FLAG_SIG, BD_DFP_HW_AVAILABLE_FLAG_SIG_LEN);
                  TR_ASSERT(dfpHWAvailableFlag, "dfpHWAvailableFlag is null in IL walker\n");
                  if (dfpHWAvailableFlag && *dfpHWAvailableFlag)
                     {
                     constToLoad = 1;
                     }
                  else if (dfpHWAvailableFlag == NULL)
                     {
                     comp()->failCompilation<TR::ILGenFailure>("dfpHWAvailableFlag is null in IL walker\n");
                     }

                  fej9()->setBigDecimalClassFieldAddr(dfpHWAvailableFlag);
                  fej9()->setCachedBigDecimalClassFieldAddr();
                  }
               }
            else
               {
               dfpHWAvailableFlag = fej9()->getBigDecimalClassFieldAddr();
               if (dfpHWAvailableFlag && *dfpHWAvailableFlag)
                  {
                  constToLoad = 1;
                  }
               else if (dfpHWAvailableFlag == NULL)
                  {
                  comp()->failCompilation<TR::ILGenFailure>("dfpHWAvailableFlag is null from cache");
                  }
               }

            loadConstant(TR::iconst, constToLoad);
            return NULL;
            }
         else if ((symbol->getRecognizedMethod() == TR::java_math_BigDecimal_DFPPerformHysteresis) ||
                 (!strncmp(calledMethod->signature(comp()->trMemory(), stackAlloc), BDDFPPERFORMHYS, BDDFPPERFORMHYSLEN)))
            {
            if ((!dfpbd && !nodfpbd) || (dfpbd && nodfpbd))
               constToLoad = 1;
            loadConstant(TR::iconst, constToLoad);
            return NULL;
            }
         else if ((symbol->getRecognizedMethod() == TR::java_math_BigDecimal_DFPUseDFP) ||
              (!strncmp(calledMethod->signature(comp()->trMemory(), stackAlloc), BDDFPUSEDFP, BDDFPUSEDFPLEN)))
            {
            if (dfpbd && !nodfpbd)
               {
               constToLoad = 1;
               loadConstant(TR::iconst, constToLoad);
               return NULL;
               }
            else if (!dfpbd && nodfpbd)
               {
               loadConstant(TR::iconst, constToLoad);
               return NULL;
               }
            }
         else if (symbol->getRecognizedMethod() == TR::com_ibm_dataaccess_DecimalData_DFPUseDFP)
            {
            if (dfpbd && !nodfpbd)
               {
               constToLoad = 1;
               loadConstant(TR::iconst, constToLoad);
               return NULL;
               }
            else if (!dfpbd && nodfpbd)
               {
               loadConstant(TR::iconst, constToLoad);
               return NULL;
               }
            else
               {
               // load the hys_type in BigDecimal
               TR_OpaqueClassBlock * clazz = fej9()->getClassFromSignature("java/math/BigDecimal", 20, comp()->getCurrentMethod());

               // DecimalData creates a BigDecimal in its <clinit>, so the class should be loaded.
               TR_ASSERT(NULL != clazz, "java/lang/BigDecimal is not loaded before com/ibm/dataaccess/DecimalData.");

               if (NULL == clazz)
                  {
                  loadConstant(TR::iconst, constToLoad);
                  return NULL;
                  }

               // Find the equivalent call in BigDecimal class, and redirect this call there.
               TR_ScratchList<TR_ResolvedMethod> bdMethods(trMemory());
               fej9()->getResolvedMethods(trMemory(), clazz, &bdMethods);
               ListIterator<TR_ResolvedMethod> bdit(&bdMethods);
               TR_ResolvedMethod * method = NULL;
               for (method = bdit.getCurrent(); method; method = bdit.getNext()) {
                  const char *sig = method->signature(comp()->trMemory());
                  int32_t len = strlen(sig);
                  if (BDDFPUSEDFPLEN == len && !strncmp(sig, BDDFPUSEDFP, BDDFPUSEDFPLEN)) {
                     break;
                     }
                  }
               TR_ASSERT(NULL != method, "Unable to find DPFUseDPF method in java/lang/BigDecimal");

               TR::ILOpCodes callOpCode = calledMethod->directCallOpCode();
               TR::Node * targetCallNode = genNodeAndPopChildren(callOpCode, 0, comp()->getSymRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, method, TR::MethodSymbol::Static));
               handleSideEffect(targetCallNode);
               genTreeTop(targetCallNode);
               push(targetCallNode);
               return targetCallNode;
               }
            }
         }

      }

    // Can't use recognized methods since it's not enabled on AOT
    //if (symbol->getRecognizedMethod() == TR::com_ibm_rmi_io_FastPathForCollocated_isVMDeepCopySupported)
    int32_t len = calledMethod->classNameLength();
    char * s = classNameToSignature(calledMethod->classNameChars(), len, comp());

    if (strstr(s, "com/ibm/rmi/io/FastPathForCollocated") &&
        !strncmp(calledMethod->nameChars(), "isVMDeepCopySupported", calledMethod->nameLength()))
       {
       loadConstant(TR::iconst, 1);
       return NULL;
       }
    else if (!strncmp(comp()->getCurrentMethod()->classNameChars(), "com/ibm/jit/JITHelpers", 22))
       {
       // fast pathing for JITHelpers
       //
       // do not do this transformation if the current method is com/ibm/jit/JITHelpers.is32Bit
       // inlineNativeCall will take care of doing the right thing
       //
       bool isCall32bit = false;
       if (strstr(s, "com/ibm/jit/JITHelpers") &&
             !strncmp(calledMethod->nameChars(), "is32Bit", calledMethod->nameLength()))
          isCall32bit = true;
#if 0
       bool fold = true;
       if (!strncmp(comp()->getCurrentMethod()->nameChars(), "is32Bit", 7))
          fold = false;

       if (fold && indirectCallFirstChild && isCall32bit)
          {
          // fold away the check if possible
          int32_t value = TR::Compiler->target.is64Bit() ? 0 : 1;
          loadConstant(TR::iconst, value);
          // cleanup the receiver because its not going to be used anymore
          //
          indirectCallFirstChild->incReferenceCount();
          indirectCallFirstChild->recursivelyDecReferenceCount();
          return NULL;
          }
#endif
       }

    if (comp()->cg()->getSupportsTMHashMapAndLinkedQueue() && (comp()->getOptions()->getGcMode() != TR_WrtbarRealTime) &&
         ((symbol->getRecognizedMethod() == TR::java_util_concurrent_ConcurrentHashMap_tmEnabled)
          || (symbol->getRecognizedMethod() == TR::java_util_concurrent_ConcurrentLinkedQueue_tmEnabled)))
       {
       loadConstant(TR::iconst, 1);
       return NULL;
       }

    if (comp()->cg()->getSupportsTMDoubleWordCASORSet() &&
          ((symbol->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicMarkableReference_tmEnabled)
          || (symbol->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicStampedReference_tmEnabled)))
       {
       loadConstant(TR::iconst, 1);
       return NULL;
       }


    bool isAMRDCAS = false;
    bool isAMRDSet = false;
    bool isASRDCAS = false;
    bool isASRDSet = false;
    char *staticName = NULL;
    int32_t staticNameLen = -1;
    char *staticSig = NULL;
    int32_t staticSigLen = -1;
    int32_t constToLoad = 0;

    if ((symbol->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCASSupported) ||
        (!strncmp(calledMethod->signature(comp()->trMemory(), stackAlloc), AMRDCASAVAIL, AMRDCASAVAILLEN)))
       {
       isAMRDCAS = true;

       staticName = DCAS_AVAILABLE_FLAG;
       staticNameLen = DCAS_AVAILABLE_FLAG_LEN;
       staticSig = DCAS_AVAILABLE_FLAG_SIG;
       staticSigLen = DCAS_AVAILABLE_FLAG_SIG_LEN;
       }
    else if ((symbol->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSetSupported) ||
             (!strncmp(calledMethod->signature(comp()->trMemory(), stackAlloc), AMRDSETAVAIL, AMRDSETAVAILLEN)))
      {
      isAMRDSet = true;

      staticName = DSET_AVAILABLE_FLAG;
      staticNameLen = DSET_AVAILABLE_FLAG_LEN;
      staticSig = DSET_AVAILABLE_FLAG_SIG;
      staticSigLen = DSET_AVAILABLE_FLAG_SIG_LEN;
      }
    else if ((symbol->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCASSupported) ||
        (!strncmp(calledMethod->signature(comp()->trMemory(), stackAlloc), ASRDCASAVAIL, ASRDCASAVAILLEN)))
       {
       isASRDCAS = true;

       staticName = DCAS_AVAILABLE_FLAG;
       staticNameLen = DCAS_AVAILABLE_FLAG_LEN;
       staticSig = DCAS_AVAILABLE_FLAG_SIG;
       staticSigLen = DCAS_AVAILABLE_FLAG_SIG_LEN;

       }
    else if ((symbol->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSetSupported) ||
             (!strncmp(calledMethod->signature(comp()->trMemory(), stackAlloc), ASRDSETAVAIL, ASRDSETAVAILLEN)))
      {
      isASRDSet = true;

      staticName = DSET_AVAILABLE_FLAG;
      staticNameLen = DSET_AVAILABLE_FLAG_LEN;
      staticSig = DSET_AVAILABLE_FLAG_SIG;
      staticSigLen = DSET_AVAILABLE_FLAG_SIG_LEN;
      }

    if (((isAMRDCAS == true) ||
         (isAMRDSet == true) ||
         (isASRDCAS == true) ||
         (isASRDSet == true)) &&
       (TR::Compiler->target.is32Bit() ||
         comp()->useCompressedPointers()))
       {
       int32_t * flagAddr = NULL;

       int32_t AMRDCASconstToLoad = -1;
       int32_t AMRDSetconstToLoad = -1;
       int32_t ASRDCASconstToLoad = -1;
       int32_t ASRDSetconstToLoad = -1;
       int32_t constToLoad = -1;

       if (isAMRDCAS)
          {
          if (AMRDCASconstToLoad == -1)
             {
             if (comp()->cg()->getSupportsDoubleWordCAS())
                AMRDCASconstToLoad = 1;
             else
                AMRDCASconstToLoad = 0;
             }
          constToLoad = AMRDCASconstToLoad;
          }
       else if (isAMRDSet)
          {
          if (AMRDSetconstToLoad == -1)
             {
             if (comp()->cg()->getSupportsDoubleWordSet())
                AMRDSetconstToLoad = 1;
             else
                AMRDSetconstToLoad = 0;
             }
          constToLoad = AMRDSetconstToLoad;
          }
       else if (isASRDCAS)
          {
          if (ASRDCASconstToLoad == -1)
             {
             if (comp()->cg()->getSupportsDoubleWordCAS())
                ASRDCASconstToLoad = 1;
             else
                ASRDCASconstToLoad = 0;
             }
          constToLoad = ASRDCASconstToLoad;
          }
       else if (isASRDSet)
          {
          if (ASRDSetconstToLoad == -1)
             {
             if (comp()->cg()->getSupportsDoubleWordSet())
                ASRDSetconstToLoad = 1;
             else
                ASRDSetconstToLoad = 0;
             }
          constToLoad = ASRDSetconstToLoad;
          }

      if (constToLoad == 1)
         {
         //TR_OpaqueClassBlock * classBlock = method()->containingClass();

         TR_OpaqueClassBlock * classBlock = NULL;
         char *fieldName;
         int32_t fieldNameLen;
         char *fieldSig;
         int32_t fieldSigLen;
         int32_t intOrBoolOffset;

         if (isAMRDCAS || isAMRDSet)
            {
            classBlock = fej9()->getClassFromSignature("Ljava/util/concurrent/atomic/AtomicMarkableReference$ReferenceBooleanPair;", 74, method());
            fieldName = "bit";
            fieldNameLen = 3;
            fieldSig = "Z";
            fieldSigLen = 1;

            intOrBoolOffset = fej9()->getObjectHeaderSizeInBytes() + fej9()->getInstanceFieldOffset(classBlock, fieldName, fieldNameLen, fieldSig, fieldSigLen);
            }
         else
            {
            classBlock = fej9()->getClassFromSignature("Ljava/util/concurrent/atomic/AtomicStampedReference$ReferenceIntegerPair;", 73, method());
            fieldName = "integer";
            fieldNameLen = 7;
            fieldSig = "I";
            fieldSigLen = 1;

            intOrBoolOffset = fej9()->getObjectHeaderSizeInBytes() + fej9()->getInstanceFieldOffset(classBlock, fieldName, fieldNameLen, fieldSig, fieldSigLen);
            }

         fieldName = "reference";
         fieldNameLen = 9;
         fieldSig = "Ljava/lang/Object;";
         fieldSigLen = 18;

         int32_t referenceOffset = fej9()->getObjectHeaderSizeInBytes() + fej9()->getInstanceFieldOffset(classBlock, fieldName, fieldNameLen, fieldSig, fieldSigLen);

         //traceMsg(comp(), "class %p reference offset %d bit offset %d\n", classBlock, referenceOffset, bitOffset);

         if ((referenceOffset != intOrBoolOffset + 4) &&
             (intOrBoolOffset != referenceOffset + TR::Compiler->om.sizeofReferenceField()))
            constToLoad = 0;
         else
            {
            if (intOrBoolOffset < referenceOffset)
               {
               if ((intOrBoolOffset % (2*TR::Compiler->om.sizeofReferenceField())) != 0)
                  constToLoad = 0;
               }
            else
               {
               if ((referenceOffset % (2*TR::Compiler->om.sizeofReferenceField())) != 0)
                  constToLoad = 0;
               }
            }
         }

      loadConstant(TR::iconst, constToLoad);
      return NULL;
      }

   if (!isStatic && _classInfo && !symRef->isUnresolved())
      {
      if (!_classInfo->getFieldInfo())
         performClassLookahead(_classInfo);

      int32_t len = calledMethod->classNameLength();
      char * s = classNameToSignature(calledMethod->classNameChars(), len, comp());

      TR::Node * thisObject = invokedynamicReceiver ? invokedynamicReceiver : _stack->element(_stack->topIndex() - (numArgs-1));
      TR_PersistentFieldInfo * fieldInfo = _classInfo->getFieldInfo() ? _classInfo->getFieldInfo()->findFieldInfo(comp(), thisObject, false) : NULL;
      if (fieldInfo && fieldInfo->isTypeInfoValid())
         {
         if (fieldInfo->getNumChars() == len && !memcmp(s, fieldInfo->getClassPointer(), len))
            {
            if (performTransformation(comp(), "O^O CLASS LOOKAHEAD: Devirtualizing call to method %s on receiver object %p which has type %.*s based on class file examination\n", calledMethod->signature(trMemory()), thisObject, len, s))
               isDirectCall = true;
            }
         }
      else if (fieldInfo &&
               fieldInfo->isBigDecimalType())
         {
         if (22 == len && !memcmp(s, "Ljava/math/BigDecimal;", len))
            {
            if (performTransformation(comp(), "O^O CLASS LOOKAHEAD: Devirtualizing call to method %s on receiver object %p which has type %.*s based on class file examination\n", calledMethod->signature(trMemory()), thisObject, len, s))
               isDirectCall = true;
            }
         }
      else if (fieldInfo &&
               fieldInfo->isBigIntegerType())
         {
         if (22 == len && !memcmp(s, "Ljava/math/BigInteger;", len))
            {
            if (performTransformation(comp(), "O^O CLASS LOOKAHEAD: Devirtualizing call to method %s on receiver object %p which has type %.*s based on class file examination\n", calledMethod->signature(trMemory()), thisObject, len, s))
               isDirectCall = true;
            }
         }
      }

   TR::Node * callNode;
   TR::Node * receiver = 0;
   if (isDirectCall)
      {
      TR::ILOpCodes callOpCode = calledMethod->directCallOpCode();
      TR::ResolvedMethodSymbol *resolvedMethodSymbol = symbol->getResolvedMethodSymbol();
      bool needToGenAndPop = false;
      if (resolvedMethodSymbol)
         {
         if (resolvedMethodSymbol->getRecognizedMethod() == TR::java_lang_Class_newInstanceImpl && // the method being called
          _methodSymbol->getRecognizedMethod() == TR::java_lang_Class_newInstance && // method we are doing genIl for
          comp()->getJittedMethodSymbol()->getRecognizedMethod() != TR::java_lang_Class_newInstance && // method we are compiling
          !isPeekingMethod() &&
          cg()->getSupportsNewInstanceImplOpt() &&
          !comp()->getOption(TR_DisableInliningOfNatives) &&
          !comp()->compileRelocatableCode() && // disable when AOTing
          !comp()->getOptions()->realTimeGC() &&
          !comp()->getOption(TR_DisableNewInstanceImplOpt)) // the caller of Class.newInstance()
            {
            TR_ASSERT(numArgs == 1, "unexpected numChildren on newInstanceImpl call");
            callNode = genNewInstanceImplCall(pop());
            calledMethod = callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->getMethod();
            }
         else
            needToGenAndPop = true;
         }
      else
         needToGenAndPop = true;

      if (needToGenAndPop)
         {
         callNode = genNodeAndPopChildren(callOpCode, numArgs, symRef);
         }
      if (!isStatic)
         receiver = callNode->getChild(0);
      if (receiver && receiver->isThisPointer())
         {
         callNode->getByteCodeInfo().setIsSameReceiver(1);
         }
      }
   else
      {
      TR::ILOpCodes callOpCode = calledMethod->indirectCallOpCode();
      if (invokedynamicReceiver)
         {
         // invokedyanmic is an oddball.  It's the only way to invoke a method
         // such that the receiver is NOT on the operand stack, yet it IS
         // included in the numArgs calculation.  That's why we pass
         // numChildren as numArgs+1 below.
         //
         callNode = genNodeAndPopChildren(callOpCode, numArgs + 1, symRef, 2);
         callNode->setAndIncChild(0, indirectCallFirstChild);
         callNode->setAndIncChild(1, invokedynamicReceiver);
         }
      else
         {
         callNode = genNodeAndPopChildren(callOpCode, numArgs + 1, symRef, 1);
         callNode->setAndIncChild(0, indirectCallFirstChild);
         }

      if (!isStatic)
         receiver = callNode->getChild(1);

      if (receiver && receiver->isThisPointer())
         {
         callNode->getByteCodeInfo().setIsSameReceiver(1);
         }
      }

   if (!comp()->getOption(TR_DisableSIMDStringCaseConv))
      {
      bool platformSupported = TR::Compiler->target.cpu.isZ();
      bool vecInstrAvailable = cg()->getSupportsVectorRegisters();

      if(symbol->getRecognizedMethod() == TR::java_lang_String_StrHWAvailable)
         {
         if (platformSupported && vecInstrAvailable)
            constToLoad = 1;
         else
            constToLoad  = 0; // should already be 0 but just incase..

         loadConstant(TR::iconst, constToLoad);
         return NULL;
         }

      if (platformSupported && vecInstrAvailable &&
            (symbol->getRecognizedMethod() == TR::java_lang_String_toUpperHWOptimizedCompressed ||
             symbol->getRecognizedMethod() == TR::java_lang_String_toLowerHWOptimizedCompressed ||
             symbol->getRecognizedMethod() == TR::java_lang_String_toUpperHWOptimizedDecompressed ||
             symbol->getRecognizedMethod() == TR::java_lang_String_toLowerHWOptimizedDecompressed ||
             symbol->getRecognizedMethod() == TR::java_lang_String_toUpperHWOptimized ||
             symbol->getRecognizedMethod() == TR::java_lang_String_toLowerHWOptimized))
         {
         isDirectCall = true;
         }
      }

   if (!comp()->getOption(TR_DisableSIMDDoubleMaxMin))
      {
      bool platformSupported = TR::Compiler->target.cpu.isZ();
      bool vecInstrAvailable = cg()->getSupportsVectorRegisters();

      if (platformSupported && vecInstrAvailable &&
            (symbol->getRecognizedMethod() == TR::java_lang_Math_max_D ||
                  symbol->getRecognizedMethod() == TR::java_lang_Math_min_D))
         {
         isDirectCall = true;
         }
      }


   if (comp()->getOptions()->getEnableGPU(TR_EnableGPU) &&
       strcmp(symbol->getMethod()->signature(trMemory()),
              "java/util/stream/IntStream.forEach(Ljava/util/function/IntConsumer;)V") == 0) // might not be resolved
      {
      //This prevents recompilation at profiled very hot which also prevents the method from reaching scorching
      //If JIT GPU works with profiled very hot or scorching in the future, this can be removed.
      TR::Recompilation *recompilationInfo = comp()->getRecompilationInfo();
      if (recompilationInfo)
         recompilationInfo->getJittedBodyInfo()->setDisableSampling(true);

      comp()->setHasIntStreamForEach();

      if (comp()->getOptLevel() < scorching &&
           !comp()->getOptimizationPlan()->getDontFailOnPurpose())
          {
          comp()->failCompilation<J9::LambdaEnforceScorching>("Enforcing optLevel=scorching");
          }
      }


   if (comp()->getOption(TR_EnableSIMDLibrary))
      {
      TR::Node * newCallNode = NULL;

      if (TR::firstVectorIntrinsic <= symbol->getRecognizedMethod() && symbol->getRecognizedMethod() <= TR::lastVectorIntrinsic)
         {
         newCallNode = handleVectorIntrinsicCall(callNode, symbol);

         if (newCallNode) // if intrinsic call is recognized
            {
            callNode = newCallNode;
            isStatic = true; // to avoid generating NULLchk
            // printf ("SIMD call recognized:  %s while compiling %s\n", symbol->getMethod()->nameChars(), comp()->signature());
            }
         }

      if (!comp()->isPeekingMethod() && !newCallNode &&
          !strncmp(symbol->getMethod()->classNameChars(), "com/ibm/dataaccess/SIMD", 23) &&
          !strncmp(symbol->getMethod()->nameChars(), "vector", 6))
         {
         printf("SIMD call not recognized:  %s while compiling %s\n", symbol->getMethod()->nameChars(), comp()->signature());
         traceMsg(comp(), "SIMD call not recognized:  %s while compiling %s\n", symbol->getMethod()->nameChars(), comp()->signature());
         }
      }

   // fast pathing for ORB readObject optimization
   //
   bool canDoSerializationOpt = true;
   if (callNode && callNode->getOpCode().hasSymbolReference() && !callNode->getSymbolReference()->isUnresolved() &&
       (_methodSymbol->getResolvedMethod()->nameLength() == ORB_CALLER_METHOD_NAME_LEN) &&
       !strncmp(_methodSymbol->getResolvedMethod()->nameChars(), ORB_CALLER_METHOD_NAME, ORB_CALLER_METHOD_NAME_LEN) &&
       (_methodSymbol->getResolvedMethod()->signatureLength() == ORB_CALLER_METHOD_SIG_LEN) &&
       !strncmp(_methodSymbol->getResolvedMethod()->signatureChars(), ORB_CALLER_METHOD_SIG, ORB_CALLER_METHOD_SIG_LEN))
      {
      if (comp()->getOption(TR_TraceILGen))
         traceMsg(comp(), "handling callNode %p, current method %s\n", callNode, _methodSymbol->getResolvedMethod()->signature(trMemory()));

      TR::Node *receiver = callNode->getFirstArgument();
      if (receiver && receiver->getOpCode().hasSymbolReference() && receiver->getSymbol()->isParm() && !receiver->isThisPointer() &&
         (calledMethod->nameLength() == ORB_CALLEE_METHOD_NAME_LEN) &&
          !strncmp(calledMethod->nameChars(), ORB_CALLEE_METHOD_NAME, ORB_CALLEE_METHOD_NAME_LEN) &&
          (calledMethod->signatureLength() == ORB_CALLEE_METHOD_SIG_LEN) &&
          !strncmp(calledMethod->signatureChars(), ORB_CALLEE_METHOD_SIG, ORB_CALLEE_METHOD_SIG_LEN))
         {
         TR_OpaqueClassBlock *cl = _methodSymbol->getResolvedMethod()->containingClass();

         if (comp()->getOption(TR_TraceILGen))
            traceMsg(comp(), "called method %s, containing class %p\n", calledMethod->signature(trMemory()), cl);

         bool isClassInitialized = false;
         TR_PersistentClassInfo * classInfo = comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(cl, comp());
         if (classInfo && classInfo->isInitialized())
             isClassInitialized = true;

         if (comp()->getOption(TR_TraceILGen))
            traceMsg(comp(), "isClassInitialized = %d\n", isClassInitialized);

         if (isClassInitialized)
            {
            TR_OpaqueClassBlock *orbClass = fej9()->getClassFromSignature(ORB_REPLACE_CLASS_NAME, ORB_REPLACE_CLASS_LEN, callNode->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod());

            if (comp()->getOption(TR_TraceILGen))
               traceMsg(comp(), "orbClass = %p, orbClassLoader %s systemClassLoader\n", orbClass, (fej9()->getClassLoader(cl) != fej9()->getSystemClassLoader()) ? "!=" : "==");

            // PR107804 if the ORB class is loaded we cannot do the serialization opt since the
            // ObjectInputStream.redirectedReadObject cannot handle ORB for some reason
            if (orbClass)
               {
               canDoSerializationOpt = false;
               }

            if (orbClass && (fej9()->getClassLoader(cl) != fej9()->getSystemClassLoader()))
               {
               TR_ScratchList<TR_ResolvedMethod> methods(trMemory());
               fej9()->getResolvedMethods(trMemory(), orbClass, &methods);
               ListIterator<TR_ResolvedMethod> it(&methods);
               TR_ResolvedMethod *replacementMethod;
               for (replacementMethod = it.getCurrent(); replacementMethod; replacementMethod = it.getNext())
                  {
                  if (replacementMethod->nameLength() == ORB_REPLACE_METHOD_NAME_LEN && !strncmp(replacementMethod->nameChars(), ORB_REPLACE_METHOD_NAME, ORB_REPLACE_METHOD_NAME_LEN))
                     {
                     if ((replacementMethod->signatureLength() == ORB_REPLACE_METHOD_SIG_LEN) &&
                         !strncmp(replacementMethod->signatureChars(), ORB_REPLACE_METHOD_SIG, ORB_REPLACE_METHOD_SIG_LEN))
                        break; // found it
                     }
                  }

               if (replacementMethod)
                  {
                  if (performTransformation(comp(), "O^O ORB OPTIMIZATION : changing ObjectInputStream.readObject call to ORB redirectedReadObject\n"))
                     {
                     TR::Node *clazzLoad = TR::Node::createWithSymRef(TR::loadaddr, 0, symRefTab()->findOrCreateClassSymbol(_methodSymbol, 0, cl));
                     TR::Node *jlClazzLoad = TR::Node::createWithSymRef(TR::aloadi, 1, 1, clazzLoad, symRefTab()->findOrCreateJavaLangClassFromClassSymbolRef());

                     push(receiver);
                     push(jlClazzLoad);
                     TR::SymbolReference *symRef = symRefTab()->findOrCreateMethodSymbol(_methodSymbol->getResolvedMethodIndex(), -1, replacementMethod, TR::MethodSymbol::Static);
                     callNode = genNodeAndPopChildren(replacementMethod->directCallOpCode(), 2, symRef);
                     isStatic = true;
                     callNode->getChild(0)->recursivelyDecReferenceCount();
                     }
                  }
               }
            }
         }
      }

   if (comp()->getOption(TR_TraceILGen))
      traceMsg(comp(), "considering callNode %p for java serialization optimization\n", callNode);
   if (canDoSerializationOpt && callNode && callNode->getOpCode().hasSymbolReference() && !callNode->getSymbolReference()->isUnresolved() &&
       callNode->getOpCode().isCallDirect())
      {
      if (comp()->getOption(TR_TraceILGen))
         traceMsg(comp(), "looking at receiver sig for callNode %p\n", callNode);
      TR::Node *receiver = callNode->getFirstArgument();
      if (receiver && receiver->getOpCode().hasSymbolReference())
         {
         TR::SymbolReference *receiverSymRef = receiver->getSymbolReference();
         int32_t receiverLen;
         const char *receiverSig = receiverSymRef->getTypeSignature(receiverLen);
         if (comp()->getOption(TR_TraceILGen))
             traceMsg(comp(), "handling callNode %p, receiver class name %s\n", callNode, receiverSig);
         if (receiverSig != NULL && (receiverLen == JAVA_SERIAL_CLASS_NAME_LEN) &&
             !strncmp(receiverSig, JAVA_SERIAL_CLASS_NAME, receiverLen))
            {
            if (comp()->getOption(TR_TraceILGen))
               traceMsg(comp(), "handling callNode %p, current method %s\n", callNode, _methodSymbol->getResolvedMethod()->signature(trMemory()));

            if ((calledMethod->nameLength() == JAVA_SERIAL_CALLEE_METHOD_NAME_LEN) &&
                !strncmp(calledMethod->nameChars(), JAVA_SERIAL_CALLEE_METHOD_NAME, JAVA_SERIAL_CALLEE_METHOD_NAME_LEN) &&
                (calledMethod->signatureLength() == JAVA_SERIAL_CALLEE_METHOD_SIG_LEN) &&
                !strncmp(calledMethod->signatureChars(), JAVA_SERIAL_CALLEE_METHOD_SIG, JAVA_SERIAL_CALLEE_METHOD_SIG_LEN))
               {
               TR_OpaqueClassBlock *cl = _methodSymbol->getResolvedMethod()->containingClass();

               if (comp()->getOption(TR_TraceILGen))
                  traceMsg(comp(), "called method %s, containing class %p\n", calledMethod->signature(trMemory()), cl);
               bool isClassInitialized = false;
               TR_PersistentClassInfo * classInfo = comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(cl, comp());
               if (classInfo && classInfo->isInitialized())
                  isClassInitialized = true;
               if (comp()->getOption(TR_TraceILGen))
                  traceMsg(comp(), "isClassInitialized = %d\n", isClassInitialized);
               if (isClassInitialized)
                  {
                  TR_OpaqueClassBlock *serialClass = fej9()->getClassFromSignature(JAVA_SERIAL_REPLACE_CLASS_NAME, JAVA_SERIAL_REPLACE_CLASS_LEN, callNode->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod());
                  if (comp()->getOption(TR_TraceILGen))
                     traceMsg(comp(), "serialClass = %p, serialClassLoader %s systemClassLoader\n", serialClass, (fej9()->getClassLoader(cl) != fej9()->getSystemClassLoader()) ? "!=" : "==");
                  if (serialClass && (fej9()->getClassLoader(cl) != fej9()->getSystemClassLoader()))
                     {
                     TR_ScratchList<TR_ResolvedMethod> methods(trMemory());
                     fej9()->getResolvedMethods(trMemory(), serialClass, &methods);
                     ListIterator<TR_ResolvedMethod> it(&methods);
                     TR_ResolvedMethod *replacementMethod;
                     for (replacementMethod = it.getCurrent(); replacementMethod; replacementMethod = it.getNext())
                        {
                        if (replacementMethod->nameLength() == JAVA_SERIAL_REPLACE_METHOD_NAME_LEN && !strncmp(replacementMethod->nameChars(), JAVA_SERIAL_REPLACE_METHOD_NAME, JAVA_SERIAL_REPLACE_METHOD_NAME_LEN))
                           {
                           if ((replacementMethod->signatureLength() == JAVA_SERIAL_REPLACE_METHOD_SIG_LEN) &&
                               !strncmp(replacementMethod->signatureChars(), JAVA_SERIAL_REPLACE_METHOD_SIG, JAVA_SERIAL_REPLACE_METHOD_SIG_LEN))
                              break; // found it
                          }
                       }
                   if (replacementMethod)
                      {
                      if (performTransformation(comp(), "O^O JAVA SERIALIZATION OPTIMIZATION : changing ObjectInputStream.readObject call to ObjectInputStream redirectedReadObject\n"))
                         {
                         TR::Node *clazzLoad = TR::Node::createWithSymRef(TR::loadaddr, 0, symRefTab()->findOrCreateClassSymbol(_methodSymbol, 0, cl));
                         TR::Node *jlClazzLoad = TR::Node::createWithSymRef(TR::aloadi, 1, clazzLoad, 0, symRefTab()->findOrCreateJavaLangClassFromClassSymbolRef());
                         push(receiver);
                         push(jlClazzLoad);
                         TR::SymbolReference *symRef = symRefTab()->findOrCreateMethodSymbol(_methodSymbol->getResolvedMethodIndex(), -1, replacementMethod, TR::MethodSymbol::Static);
                         callNode = genNodeAndPopChildren(replacementMethod->directCallOpCode(), 2, symRef);
                         isStatic = true;
                         callNode->getChild(0)->recursivelyDecReferenceCount();
                         }
                      }
                   }
                }
              }
            }
          }
       }

   TR::Node *treeTopNode;
   if (isStatic || callNode->getChild(callNode->getFirstArgumentIndex())->isNonNull())
      {
      if (symRef->isUnresolved())
         treeTopNode = genResolveCheck(callNode);
      else
         treeTopNode = callNode;
      }
   else
      {
      if (symRef->isUnresolved())
         treeTopNode = genResolveAndNullCheck(callNode);
      else
         treeTopNode = genNullCheck(callNode);
      }

   handleSideEffect(treeTopNode);

   TR::TreeTop *callNodeTreeTop = NULL;
   if (symbol->getMandatoryRecognizedMethod() == TR::java_lang_invoke_ILGenMacros_placeholder)
      {
      // This call is not a real Java call.  We can't put down a treetop for
      // it, or else that treetop will linger after the placeholder has been
      // expanded, at which point the placeholder call's children will all have
      // the wrong refcounts.
      }
   else
      {
      if (!_intrinsicErrorHandling)
         {
         callNodeTreeTop = genTreeTop(treeTopNode);
         }
      else
         {
         callNodeTreeTop = TR::TreeTop::create(comp(), treeTopNode);
         }
         _intrinsicErrorHandling = false;
      }
   TR::Node * resultNode = 0;

   TR::ResolvedMethodSymbol * resolvedMethodSymbol = symbol->getResolvedMethodSymbol();


   // fast pathing for JITHelpers methods
   //
   if (!strncmp(comp()->getCurrentMethod()->classNameChars(), "com/ibm/jit/JITHelpers", 22))
      {
      bool isCallGetLength = false;
      bool isCallAddressAsPrimitive32 = false;
      bool isCallAddressAsPrimitive64 = false;
      if (strstr(s, "java/lang/reflect/Array") &&
            !strncmp(calledMethod->nameChars(), "getLength", calledMethod->nameLength()))
         isCallGetLength = true;
      ///else if (strstr(s, "com/ibm/jit/JITHelpers") && resolvedMethodSymbol)
      else if (resolvedMethodSymbol)
         {
         ///         (!strncmp(calledMethod->nameChars(), "getAddressAsPrimitive32", calledMethod->nameLength()) ||
         ///          !strncmp(calledMethod->nameChars(), "getAddressAsPrimitive64", calledMethod->nameLength())))
         if (resolvedMethodSymbol->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_getAddressAsPrimitive32)
            isCallAddressAsPrimitive32 = true;
         else if (resolvedMethodSymbol->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_getAddressAsPrimitive64)
            isCallAddressAsPrimitive64 = true;
         }

      ///if (!strncmp(comp()->getCurrentMethod()->nameChars(), "hashCodeImpl", 12) &&
      ///       isCallGetLength)
      if (_methodSymbol->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_hashCodeImpl)
         {
         if (isCallGetLength)
            {
            // fold away Array.getLength because its guaranteed that the parm is an array
            //
            TR::Node::recreate(callNode, TR::arraylength);
            callNode->setArrayStride(sizeof(intptrj_t));
            if (treeTopNode->getOpCode().isResolveOrNullCheck())
               {
               TR::Node::recreate(treeTopNode, TR::NULLCHK);
               treeTopNode->setSymbolReference(comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(_methodSymbol));
               }
            }
         else if (isCallAddressAsPrimitive32)
            {
            // the first child is a instance of JITHelpers, should be removed since the new node is no longer a method call
            callNode->removeChild(0);
            TR::Node::recreate(callNode, TR::a2i);
            }
         else if (isCallAddressAsPrimitive64)
            {
            // the first child is a instance of JITHelpers, should be removed since the new node is no longer a method call
            callNode->removeChild(0);
            TR::Node::recreate(callNode, TR::a2l);
            }
         }
      }

   if (resolvedMethodSymbol &&
       resolvedMethodSymbol->getRecognizedMethod() == TR::java_lang_String_indexOf_String &&
       !isPeekingMethod() &&
       !comp()->getOption(TR_DisableInliningOfNatives) &&
       !comp()->getOption(TR_DisableFastStringIndexOf))
      {
      resultNode = TR::TransformUtil::transformStringIndexOfCall(comp(), callNode);
      if (resultNode != callNode)
         {
         if (treeTopNode->getOpCode().isCheck())
            {
            // the orignal 'this' of the indexOf was being nullchk'd (note: it cannot be a resolv check)
            // nullchk the original nullchk reference, and create the static call nestled under a treetop
            //
            TR::Node *passThrough = TR::Node::create(TR::PassThrough, 1, callNode->getFirstChild());
            callNodeTreeTop->getNode()->setAndIncChild(0, passThrough);
            callNodeTreeTop = genTreeTop(callNode = resultNode);
            callNode->decReferenceCount();
            }
         else
            callNodeTreeTop->getNode()->setChild(0, callNode = resultNode);
         }
      }
   else if (resolvedMethodSymbol &&
       !isPeekingMethod() &&
       (resolvedMethodSymbol->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_getJ9ClassFromObject32 ||
        resolvedMethodSymbol->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_getJ9ClassFromObject64))
      {
      TR::Node* obj = callNode->getChild(1);
      TR::Node* vftLoad = TR::Node::createWithSymRef(callNode, TR::aloadi, 1, obj, symRefTab()->findOrCreateVftSymbolRef());
      if (resolvedMethodSymbol->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_getJ9ClassFromObject32)
         {
         resultNode = TR::Node::create(callNode, TR::a2i, 1, vftLoad);
         }
      else
         {
         resultNode = TR::Node::create(callNode, TR::a2l, 1, vftLoad);
         }
      // Handle NullCHK
      if (callNodeTreeTop->getNode()->getOpCode().isNullCheck())
         TR::Node::recreate(callNodeTreeTop->getNode(), TR::treetop);
      callNodeTreeTop->getNode()->setAndIncChild(0, resultNode);
      // Decrement ref count for the call
      callNode->recursivelyDecReferenceCount();
      callNode = resultNode;
      }
   else if (resolvedMethodSymbol &&
       !isPeekingMethod() &&
       resolvedMethodSymbol->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_isArray)
      {
      TR::Node* obj = callNode->getChild(1);
      TR::Node* vftLoad = TR::Node::createWithSymRef(callNode, TR::aloadi, 1, obj, symRefTab()->findOrCreateVftSymbolRef());

      if (TR::Compiler->target.is32Bit())
         {
         resultNode = TR::Node::createWithSymRef(callNode, TR::iloadi, 1, vftLoad, symRefTab()->findOrCreateClassAndDepthFlagsSymbolRef());
         }
      else
         {
         resultNode = TR::Node::createWithSymRef(callNode, TR::lloadi, 1, vftLoad, symRefTab()->findOrCreateClassAndDepthFlagsSymbolRef());
         resultNode = TR::Node::create(callNode, TR::l2i, 1, resultNode);
         }

      int32_t andMask = comp()->fej9()->getFlagValueForArrayCheck();
      int32_t shiftAmount = trailingZeroes(andMask);
      resultNode  = TR::Node::create(callNode, TR::iand, 2, resultNode, TR::Node::iconst(callNode, andMask));
      resultNode  = TR::Node::create(callNode, TR::iushr, 2, resultNode, TR::Node::iconst(callNode, shiftAmount));
      // Handle NullCHK
      if (callNodeTreeTop->getNode()->getOpCode().isNullCheck())
         TR::Node::recreate(callNodeTreeTop->getNode(), TR::treetop);
      callNodeTreeTop->getNode()->setAndIncChild(0, resultNode);
      // Decrement ref count for the call
      callNode->recursivelyDecReferenceCount();
      callNode = resultNode;
      }
   else if (resolvedMethodSymbol &&
       resolvedMethodSymbol->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_getClassInitializeStatus &&
       !isPeekingMethod())
      {
      TR::Node* jlClass = callNode->getChild(1);
      TR::Node* j9Class = TR::Node::createWithSymRef(callNode, TR::aloadi, 1, jlClass, symRefTab()->findOrCreateClassFromJavaLangClassSymbolRef());

      if (TR::Compiler->target.is32Bit())
         {
         resultNode = TR::Node::createWithSymRef(callNode, TR::iloadi, 1, j9Class, symRefTab()->findOrCreateInitializeStatusFromClassSymbolRef());
         }
      else
         {
         resultNode = TR::Node::createWithSymRef(callNode, TR::lloadi, 1, j9Class, symRefTab()->findOrCreateInitializeStatusFromClassSymbolRef());
         resultNode = TR::Node::create(callNode, TR::l2i, 1, resultNode);
         }

      // Properly handle the checks
      if (callNodeTreeTop->getNode()->getOpCode().isNullCheck())
         TR::Node::recreate(callNodeTreeTop->getNode(), TR::treetop);
      callNodeTreeTop->getNode()->setAndIncChild(0, resultNode);
      // Decrement ref count for the call
      callNode->recursivelyDecReferenceCount();
      callNode = resultNode;
      }
   else if (symbol->isNative() && isDirectCall)
      {
      if (!comp()->getOption(TR_DisableInliningOfNatives) &&
          symbol->castToResolvedMethodSymbol()->getRecognizedMethod() != TR::unknownMethod)
         {
         if (!resultNode)
            {
            resultNode = fej9()->inlineNativeCall(comp(), callNodeTreeTop, callNode);
            }
         }

      if (!resultNode)
         {
         if (symbol->isJNI())
            resultNode = callNode->processJNICall(callNodeTreeTop, _methodSymbol);
         else
            resultNode = callNode;
         }
      }
   else
      resultNode = callNode;

   TR::DataType returnType = calledMethod->returnType();
   if (returnType != TR::NoType)
      {
      push(resultNode);
      }

   // callNodeTreeTop may be null if this call should not be placed in the trees
   if (callNodeTreeTop
       && comp()->getOption(TR_EnableOSR)
       && !comp()->isPeekingMethod()
       && comp()->isOSRTransitionTarget(TR::postExecutionOSR)
       && comp()->isPotentialOSRPoint(callNodeTreeTop->getNode())
       && !_methodSymbol->cannotAttemptOSRAt(callNode->getByteCodeInfo(), NULL, comp())
       && !_cannotAttemptOSR)
      {
      saveStack(-1, !comp()->pendingPushLivenessDuringIlgen());
      stashPendingPushLivenessForOSR(comp()->getOSRInductionOffset(callNode));
      }

   if (cg()->getEnforceStoreOrder() && calledMethod->isConstructor())
      {
      if (resolvedMethodSymbol)
         {
         J9Class *methodClass = (J9Class *) resolvedMethodSymbol->getResolvedMethod()->containingClass();
         TR_VMFieldsInfo *fieldsInfoByIndex = new (comp()->trStackMemory()) TR_VMFieldsInfo(comp(), methodClass, 1, stackAlloc);
         ListIterator<TR_VMField> fieldIter(fieldsInfoByIndex->getFields());
         for (TR_VMField *field = fieldIter.getFirst(); field; field = fieldIter.getNext())
            {
            if ((field->modifiers & J9AccFinal) && !isFinalFieldFromSuperClasses(methodClass, field->offset))
               {
               push(callNode->getFirstChild());
               genFlush(0);
               pop();
               break;
               }
            }
         }
      else
         {
         push(callNode->getFirstChild());
         genFlush(0);
         pop();
         }
      }

   if (isDirectCall && indirectCallFirstChild)
      {
      // Not using the supplied node; must clean up its ref counts
      indirectCallFirstChild->incReferenceCount();
      indirectCallFirstChild->recursivelyDecReferenceCount();
      }

   _intrinsicErrorHandling = false;
   return callNode;
   }

void
TR_J9ByteCodeIlGenerator::chopPlaceholder(TR::Node *placeholder, int32_t firstChild, int32_t numChildren)
   {
   // Dec refcounts on the children we're going to drop.  Also, find
   // start and end of the portion of the placeholder signature
   // describing the arguments we're going to keep.
   //
   int32_t i;
   for (i = 0; i < firstChild; i++)
      placeholder->getAndDecChild(i);
   for (i = placeholder->getNumChildren()-1; i >= firstChild + numChildren; i--)
      placeholder->getAndDecChild(i);

   // Move the remaining children to the front.
   //
   for (i = 0; i < numChildren; i++)
      placeholder->setChild(i, placeholder->getChild(i + firstChild));
   placeholder->setNumChildren(numChildren);

   // Edit signature
   //
   char *callSignature = placeholder->getSymbol()->castToMethodSymbol()->getMethod()->signatureChars();
   placeholder->setSymbolReference(symRefWithArtificialSignature(placeholder->getSymbolReference(),
      "(.-).$",
      callSignature, firstChild, firstChild + numChildren - 1,
      callSignature
      ));
   }

bool
TR_J9ByteCodeIlGenerator::runMacro(TR::SymbolReference * symRef)
   {
   if (comp()->isPeekingMethod())
      {
      // Not safe to run ILGen macros when peeking
      return false;
      }

   // Give FE first kick at the can
   //
   if (runFEMacro(symRef))
      return true;

   TR::MethodSymbol * symbol = symRef->getSymbol()->castToMethodSymbol();
   int32_t archetypeParmCount = symbol->getMethod()->numberOfExplicitParameters() + (symbol->isStatic() ? 0 : 1);

   TR_ASSERT(symbol->isStatic(), "Macro methods must be static or else signature processing gets complicated by the implicit receiver");

   switch (symRef->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
      {
      case TR::java_lang_invoke_ILGenMacros_numArguments:
         {
         if (!comp()->compileRelocatableCode())
            {
            TR::Node *argShepherd = genNodeAndPopChildren(TR::icall, 1, symRef);
            loadConstant(TR::iconst, argShepherd->getNumChildren());
            argShepherd->removeAllChildren();
            }
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_populateArray:
         {
         if (!comp()->compileRelocatableCode())
            {
            TR::Node *argShepherd = genNodeAndPopChildren(TR::icall, 1, symRef);
            TR::Node *array = pop();
            for (int32_t i = 0; i < argShepherd->getNumChildren(); i++)
               {
               TR::Node *arg = argShepherd->getChild(i);
               push(array);
               loadConstant(TR::iconst, i);
               push(arg);
               storeArrayElement(arg->getDataType()); // TODO:JSR292: use icstore for char arguments
               }
            argShepherd->removeAllChildren();
            push(array);
            }
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_first:
         {
         if (!comp()->compileRelocatableCode())
            {
            TR::Node *placeholder = genNodeAndPopChildren(TR::icall, 1, placeholderWithDummySignature());
            chopPlaceholder(placeholder, 0, 1);
            push(placeholder);
            }
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_last:
         {
         if (!comp()->compileRelocatableCode())
            {
            TR::Node *placeholder = genNodeAndPopChildren(TR::icall, 1, placeholderWithDummySignature());
            chopPlaceholder(placeholder, placeholder->getNumChildren()-1, 1);
            push(placeholder);
            }
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_firstN:
         {
         if (!comp()->compileRelocatableCode())
            {
            TR::Node *placeholder = genNodeAndPopChildren(TR::icall, 1, placeholderWithDummySignature());
            int32_t n = pop()->getInt();
            chopPlaceholder(placeholder, 0, n);
            push(placeholder);
            }
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_dropFirstN:
         {
         if (!comp()->compileRelocatableCode())
            {
            TR::Node *placeholder = genNodeAndPopChildren(TR::icall, 1, placeholderWithDummySignature());
            int32_t n = pop()->getInt();
            chopPlaceholder(placeholder, n, placeholder->getNumChildren()-n);
            push(placeholder);
            }
         return true;
         }
       case TR::java_lang_invoke_ILGenMacros_lastN:
         {
         if (!comp()->compileRelocatableCode())
            {
            TR::Node *placeholder = genNodeAndPopChildren(TR::icall, 1, placeholderWithDummySignature());
            int32_t n = pop()->getInt();
            chopPlaceholder(placeholder, placeholder->getNumChildren()-n, n);
            push(placeholder);
            }
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_middleN:
         {
         if (!comp()->compileRelocatableCode())
            {
            TR::Node *placeholder = genNodeAndPopChildren(TR::icall, 1, placeholderWithDummySignature());
            int32_t n          = pop()->getInt();
            int32_t startIndex = pop()->getInt();
            chopPlaceholder(placeholder, startIndex, n);
            push(placeholder);
            }
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_rawNew:
         {
         if (!comp()->compileRelocatableCode())
            {
            push(TR::Node::createWithSymRef(TR::aloadi, 1, 1, pop(), symRefTab()->findOrCreateClassFromJavaLangClassSymbolRef()));
            genNew(TR::variableNew);
            }
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_push:
         {
         if (!comp()->compileRelocatableCode())
            shiftAndCopy(_stack->size(), 1);
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_pop:
         {
         if (!comp()->compileRelocatableCode())
            rotate(-1);
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_invokeExact:
         {
         if (!comp()->compileRelocatableCode())
            {
            push(_stack->element(_stack->size() - archetypeParmCount)); // dup receiver
            TR_ResolvedMethod  *invokeExactMacro = symRef->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();
            TR::SymbolReference *invokeExact = comp()->getSymRefTab()->methodSymRefFromName(_methodSymbol, JSR292_MethodHandle, JSR292_invokeExact, JSR292_invokeExactSig, TR::MethodSymbol::ComputedVirtual);
            TR::SymbolReference *invokeExactWithSig = symRefWithArtificialSignature(invokeExact,
               "(.*).$",
               invokeExactMacro->signatureChars(), 1, // skip explicit MethodHandle argument -- invokeExact has it as a receiver
               invokeExactMacro->signatureChars());
            genInvokeHandle(invokeExactWithSig);
            }
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_typeCheck:
         {
         if (!comp()->compileRelocatableCode())
            genHandleTypeCheck();
         return true;
         }
      case TR::java_lang_invoke_ILGenMacros_arrayElements:
         {
         if (!comp()->compileRelocatableCode())
            {
            TR::Node *argPlaceholder = genNodeAndPopChildren(TR::icall, 3, symRef);
            TR::Node *array  = argPlaceholder->getAndDecChild(0);
            int firstIndex  = argPlaceholder->getAndDecChild(1)->getInt();
            int numElements = argPlaceholder->getAndDecChild(2)->getInt();

            // The hard part here is computing the signature for the resulting placeholder!
            char *macroSignature = argPlaceholder->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->signatureChars();
            char *arrayElementType = macroSignature+2; // Skip parenthesis and bracket
            char *secondArgType = nextSignatureArgument(arrayElementType);
            int arrayElementTypeLength = secondArgType - arrayElementType;

            char *expandedArgsSignature = (char*)comp()->trMemory()->allocateStackMemory(numElements * arrayElementTypeLength + 1);

            TR::DataType arrayElementDataType = TR::NoType;
            TR::ILOpCodes convertOp = TR::BadILOp;
            switch (arrayElementType[0])
               {
               case 'Z':
               case 'B':
                  arrayElementDataType = TR::Int8;
                  convertOp = TR::b2i;
                  break;
               case 'S':
                  arrayElementDataType = TR::Int16;
                  convertOp = TR::s2i;
                  break;
               case 'C':
                  arrayElementDataType = TR::Int16;
                  convertOp = TR::su2i;
                  break;
               case 'I':
                  arrayElementDataType = TR::Int32;
                  break;
               case 'J':
                  arrayElementDataType = TR::Int64;
                  break;
               case 'F':
                  arrayElementDataType = TR::Float;
                  break;
               case 'D':
                  arrayElementDataType = TR::Double;
                  break;
               case 'L':
               case '[':
                  arrayElementDataType = TR::Address;
                  break;
               default:
                  TR_ASSERT(0, "Unknown array element type '%s'", arrayElementType);
                  arrayElementDataType = TR::Address;
                  break;
               }

            char *cursor = expandedArgsSignature;
            cursor[0] = 0; // in case numElements==0
            for (int32_t i = firstIndex; i < firstIndex+numElements; i++)
               {
               push(array);
               loadConstant(TR::iconst, i);
               loadArrayElement(arrayElementDataType);
               if (convertOp != TR::BadILOp)
                  genUnary(convertOp);
               cursor += sprintf(cursor, "%.*s", arrayElementTypeLength, arrayElementType);
               }

            // Create placeholder with signature that reflects the expansion of arguments.
            // Being a placeholder, its return type is still int.
            //
            TR::Node *placeholder = genNodeAndPopChildren(TR::icall, numElements, symRefWithArtificialSignature(placeholderWithDummySignature(),
               "(.?)I", expandedArgsSignature));

            push(placeholder);
            }
         return true;
         }
      default:
         return false;
      }
   }

//----------------------------------------------
// gen load
//----------------------------------------------

void
TR_J9ByteCodeIlGenerator::loadAuto(TR::DataType type, int32_t slot, bool isAdjunct)
   {
   if (_argPlaceholderSlot != -1 && _argPlaceholderSlot == slot)
      {
      genArgPlaceholderCall();
      return;
      }

   TR::Node * load = TR::Node::createLoad(symRefTab()->findOrCreateAutoSymbol(_methodSymbol, slot, type, true, false, true, isAdjunct));
   // type may have been coerced
   type = load->getDataType();

   bool isStatic = _methodSymbol->isStatic();
   if (slot == 0 && !isStatic && !_thisChanged)
      load->setIsNonNull(true);

   push(load);
   }


void
TR_J9ByteCodeIlGenerator::loadInstance(int32_t cpIndex)
   {
   TR::SymbolReference * symRef = symRefTab()->findOrCreateShadowSymbol(_methodSymbol, cpIndex, false);
   TR::Symbol * symbol = symRef->getSymbol();
   TR::DataType type = symbol->getDataType();

   TR::Node * address = pop();

   if (!symRef->isUnresolved() &&
       symRef->getSymbol()->isFinal())
      {
      TR::Node *constValue =  loadConstantValueIfPossible(address, symRef->getOffset(), type, false);
      if (constValue)
         {
         return;
         }
      }

   TR::Node * load, *dummyLoad;
   dummyLoad = load = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectLoad(type), 1, 1, address, symRef);

   // loading cleanroom BigDecimal long field?
   // performed only when DFP isn't disabled, and the target
   // is DFP enabled (i.e. Power6, zSeries6)
   if (!comp()->compileRelocatableCode() && !comp()->getOption(TR_DisableDFP) &&
       ((TR::Compiler->target.cpu.isPower() && TR::Compiler->target.cpu.supportsDecimalFloatingPoint())
#ifdef TR_TARGET_S390
         || (TR::Compiler->target.cpu.isZ() && TR::Compiler->target.cpu.getS390SupportsDFP())
#endif
         ))
      {
      char * className = NULL;
      className = _methodSymbol->getResolvedMethod()->classNameChars();
      if (className != NULL && BDCLASSLEN == strlen(className) &&
             !strncmp(className, BDCLASS, BDCLASSLEN))
         {
         int32_t fieldLen=0;
         char * fieldName =  _methodSymbol->getResolvedMethod()->fieldNameChars(cpIndex, fieldLen);
         if (fieldName != NULL && BDFIELDLEN == strlen(fieldName) && !strncmp(fieldName, BDFIELD, BDFIELDLEN))
            {
            load->setIsBigDecimalLoad();
            comp()->setContainsBigDecimalLoad(true);
            }
         }
      }
   TR::Node * treeTopNode = 0;

   if (symRef->isUnresolved())
      {
      if (!address->isNonNull())
         treeTopNode = genResolveAndNullCheck(load);
      else
         treeTopNode = genResolveCheck(load);
      }
   else if (!address->isNonNull())
      treeTopNode = genNullCheck(load);
   else if (symbol->isVolatile())
      treeTopNode = load;

   if (treeTopNode)
      {
      handleSideEffect(treeTopNode);
      genTreeTop(treeTopNode);
      }

   if (type == TR::Address)
      {

      if (comp()->useCompressedPointers())
         {
         if (!symRefTab()->isFieldClassObject(symRef))
            {
            TR::Node *loadValue = load;
            if (loadValue->getOpCode().isCheck())
               loadValue = loadValue->getFirstChild();
            // returns non-null if the compressedRefs anchor is going to
            // be part of the subtrees (for now, it is a treetop)
            //
            TR::Node *newLoad = genCompressedRefs(loadValue);
            if (newLoad)
               load = newLoad;
            }
         }
      }

   push(dummyLoad);
   }

void
TR_J9ByteCodeIlGenerator::loadStatic(int32_t cpIndex)
   {
   _staticFieldReferenceEncountered = true;
   TR::SymbolReference * symRef = symRefTab()->findOrCreateStaticSymbol(_methodSymbol, cpIndex, false);
   TR::StaticSymbol *      symbol = symRef->getSymbol()->castToStaticSymbol();
   TR_ASSERT(symbol, "Didn't geta static symbol.");

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fej9());

   if (!comp()->isDLT())
      {
      TR::Symbol::RecognizedField recognizedField = symbol->getRecognizedField();
      switch (recognizedField)
         {
         case TR::Symbol::Java_math_BigInteger_useLongRepresentation:
            {
            // always evaluate to be true
            loadConstant(TR::iconst, 1);
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_IS_32_BIT:
            {
            int32_t constValue = TR::Compiler->target.is64Bit() ? 0 : 1;
            loadConstant(TR::iconst, constValue);
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9OBJECT_J9CLASS_OFFSET:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getOffsetOfJ9ObjectJ9Class());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getObjectHeaderHasBeenMovedInClass());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getObjectHeaderHasBeenHashedInClass());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9OBJECT_FLAGS_MASK32:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getJ9ObjectFlagsMask32());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9OBJECT_FLAGS_MASK64:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getJ9ObjectFlagsMask64());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_JLTHREAD_J9THREAD_OFFSET:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getOffsetOfJLThreadJ9Thread());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9THREAD_J9VM_OFFSET:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getOffsetOfJ9ThreadJ9VM());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9ROMARRAYCLASS_ARRAYSHAPE_OFFSET:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getOffsetOfJ9ROMArrayClassArrayShape());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9CLASS_BACKFILL_OFFSET_OFFSET:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getOffsetOfBackfillOffsetField());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_ARRAYSHAPE_ELEMENTCOUNT_MASK:
            {
            loadConstant(TR::iconst, 65535);
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9CONTIGUOUSARRAY_HEADER_SIZE:
            {
            loadConstant(TR::iconst, (int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9DISCONTIGUOUSARRAY_HEADER_SIZE:
            {
            loadConstant(TR::iconst, (int32_t)TR::Compiler->om.discontiguousArrayHeaderSizeInBytes());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9OBJECT_CONTIGUOUS_LENGTH_OFFSET:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getJ9ObjectContiguousLength());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9OBJECT_DISCONTIGUOUS_LENGTH_OFFSET:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getJ9ObjectDiscontiguousLength());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_JLOBJECT_ARRAY_BASE_OFFSET:
            {
            loadConstant(TR::iconst, 8);
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9CLASS_J9ROMCLASS_OFFSET:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getOffsetOfClassRomPtrField());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9JAVAVM_IDENTITY_HASH_DATA_OFFSET:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getOffsetOfJavaVMIdentityHashData());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9IDENTITYHASHDATA_HASH_DATA1_OFFSET:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getOffsetOfJ9IdentityHashData1());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9IDENTITYHASHDATA_HASH_DATA2_OFFSET:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getOffsetOfJ9IdentityHashData2());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9IDENTITYHASHDATA_HASH_DATA3_OFFSET:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getOffsetOfJ9IdentityHashData3());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9IDENTITYHASHDATA_HASH_SALT_TABLE_OFFSET:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getOffsetOfJ9IdentityHashDataHashSaltTable());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9_IDENTITY_HASH_SALT_POLICY_STANDARD:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getJ9IdentityHashSaltPolicyStandard());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9_IDENTITY_HASH_SALT_POLICY_REGION:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getJ9IdentityHashSaltPolicyRegion());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_J9_IDENTITY_HASH_SALT_POLICY_NONE:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getJ9IdentityHashSaltPolicyNone());
            return;
            }
         case TR::Symbol::Com_ibm_jit_JITHelpers_IDENTITY_HASH_SALT_POLICY:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getIdentityHashSaltPolicy());
            return;
            }
         case TR::Symbol::Com_ibm_oti_vm_VM_J9CLASS_CLASS_FLAGS_OFFSET:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getOffsetOfClassFlags());
            return;
            }
         case TR::Symbol::Com_ibm_oti_vm_VM_J9CLASS_INITIALIZE_STATUS_OFFSET:
            {

            loadConstant(TR::iconst, (int32_t)fej9->getOffsetOfClassInitializeStatus());
            return;
            }
         case TR::Symbol::Com_ibm_oti_vm_VM_J9_JAVA_CLASS_RAM_SHAPE_SHIFT:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getJ9JavaClassRamShapeShift());
            return;
            }
         case TR::Symbol::Com_ibm_oti_vm_VM_OBJECT_HEADER_SHAPE_MASK:
            {
            loadConstant(TR::iconst, (int32_t)fej9->getObjectHeaderShapeMask());
            return;
            }
         case TR::Symbol::Com_ibm_oti_vm_VM_ADDRESS_SIZE:
            {
            loadConstant(TR::iconst, (int32_t)sizeof(uintptrj_t));
            return;
            }
         default:
         	break;
         }
      }

   TR::DataType type = symbol->getDataType();
   bool isResolved = !symRef->isUnresolved();
   TR_OpaqueClassBlock * classOfStatic = isResolved ? _method->classOfStatic(cpIndex) : 0;
   if (classOfStatic == NULL)
      {
      int         len = 0;
      char * classNameOfFieldOrStatic = NULL;
      classNameOfFieldOrStatic = symRef->getOwningMethod(comp())->classNameOfFieldOrStatic(symRef->getCPIndex(), len);
      if (classNameOfFieldOrStatic)
         {
         classNameOfFieldOrStatic=classNameToSignature(classNameOfFieldOrStatic, len, comp());
         TR_OpaqueClassBlock * curClass = fej9->getClassFromSignature(classNameOfFieldOrStatic, len, symRef->getOwningMethod(comp()));
         TR_OpaqueClassBlock * owningClass = comp()->getJittedMethodSymbol()->getResolvedMethod()->containingClass();
         if (owningClass == curClass)
            classOfStatic = curClass;
         }
      }


   bool isClassInitialized = false;
   TR_PersistentClassInfo * classInfo = _noLookahead ? 0 :
      comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(classOfStatic, comp());
   if (classInfo && classInfo->isInitialized())
      isClassInitialized = true;

   /*
   if (classOfStatic)
      {
      char *name; int32_t len;
      name = comp()->fej9->getClassNameChars(classOfStatic, len);
      printf("class name %s class init %d class %p classInfo %p no %d hotness %d\n", name, isClassInitialized, classOfStatic, classInfo, _noLookahead, comp()->getMethodHotness()); fflush(stdout);
      }
   */

   bool canOptimizeFinalStatic = false;
   if (isResolved && symbol->isFinal() && !symRef->isUnresolved() &&
       classOfStatic != comp()->getSystemClassPointer() &&
       isClassInitialized &&
       !comp()->compileRelocatableCode())
      {
      //if (type == TR::Address)
         {

         // todo: figure out why classInfo would be NULL here?
         if (!classInfo->getFieldInfo())
            performClassLookahead(classInfo);
         }

      if (classInfo->getFieldInfo() && !classInfo->cannotTrustStaticFinal())
         canOptimizeFinalStatic = true;
      }

   TR::VMAccessCriticalSection loadStaticCriticalSection(fej9,
                                                          TR::VMAccessCriticalSection::tryToAcquireVMAccess,
                                                          comp());

   if (canOptimizeFinalStatic &&
       loadStaticCriticalSection.hasVMAccess())
      {
      void * p = symbol->getStaticAddress();

      switch (type)
         {
         case TR::Address:
            if (*(void **)p == 0)
               {
               loadConstant(TR::aconst, 0);
               break;
               }
            else
               {
               // the address isn't constant, because a GC could move it, however is it nonnull
               //
               TR::Node * node = TR::Node::createLoad(symRef);
               node->setIsNonNull(true);
               push(node);
               break;
               }
         case TR::Double:  loadConstant(TR::dconst, *(double *) p); break;
         case TR::Int64:   loadConstant(TR::lconst, *(int64_t *)p); break;
         case TR::Float:   loadConstant(TR::fconst, *(float *)  p); break;
         case TR::Int32:
         default:         loadConstant(TR::iconst, *(int32_t *)p); break;
         }
      }
   else if (symbol->isVolatile() && type == TR::Int64 && isResolved && TR::Compiler->target.is32Bit() &&
            !comp()->cg()->getSupportsInlinedAtomicLongVolatiles() && 0)
      {
      TR::SymbolReference * volatileLongSymRef =
          comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_volatileReadLong, false, false, true);
      TR::Node * statics = TR::Node::createWithSymRef(TR::loadaddr, 0, symRef);
      TR::Node * call = TR::Node::createWithSymRef(TR::lcall, 1, 1, statics, volatileLongSymRef);

      handleSideEffect(call);

      genTreeTop(call);
      push(call);
      }
   else
      {
      TR::Node * load;
      if (cg()->getAccessStaticsIndirectly() && isResolved && type != TR::Address && !comp()->compileRelocatableCode())
         {
         TR::Node * statics = TR::Node::createWithSymRef(TR::loadaddr, 0, symRefTab()->findOrCreateClassStaticsSymbol(_methodSymbol, cpIndex));
         load = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectLoad(type), 1, 1, statics, symRef);
         }
      else
         load = TR::Node::createWithSymRef(comp()->il.opCodeForDirectLoad(type), 0, symRef);

      TR::Node * treeTopNode = 0;
      if (symRef->isUnresolved())
         treeTopNode = genResolveCheck(load);
      else if (symbol->isVolatile())
         treeTopNode = load;

      if (treeTopNode)
         {
         handleSideEffect(treeTopNode);
         genTreeTop(treeTopNode);
         }

      push(load);
      }
   }

TR::Node *
TR_J9ByteCodeIlGenerator::loadSymbol(TR::ILOpCodes loadop, TR::SymbolReference * symRef)
   {
   TR::Node * node = TR::Node::createWithSymRef(loadop, 0, symRef);

   if (symRef->isUnresolved())
      {
      TR::Node * treeTopNode = genResolveCheck(node);
      handleSideEffect(treeTopNode);
      genTreeTop(treeTopNode);
      }

   push(node);
   return node;
   }

void
TR_J9ByteCodeIlGenerator::loadClassObject(int32_t cpIndex)
   {
   void * classObject = method()->getClassFromConstantPool(comp(), cpIndex);
   loadSymbol(TR::loadaddr, symRefTab()->findOrCreateClassSymbol(_methodSymbol, cpIndex, classObject));
   }

/**
 * Push a loadaddr of the class for cpIndex without emitting ResolveCHK.
 *
 * This is needed for checkcast and instanceof bytecode instructions, which are
 * required not to resolve classes when the tested object is null, so they
 * can't run ResolveCHK unconditionally.
 *
 * @param cpIndex The index of the constant pool entry that specifies the class
 * @param aotInhibit The JIT option to disallow using the resolved class for AOT
 * @return The symbol reference for the class
 *
 * @see genCheckCast(int32_t)
 * @see genInstanceof(int32_t)
 */
TR::SymbolReference *
TR_J9ByteCodeIlGenerator::loadClassObjectForTypeTest(int32_t cpIndex, TR_CompilationOptions aotInhibit)
   {
   bool aotOK = !comp()->compileRelocatableCode() || !comp()->getOption(aotInhibit);
   void *classObject = method()->getClassFromConstantPool(comp(), cpIndex, aotOK);
   TR::SymbolReference *symRef = symRefTab()->findOrCreateClassSymbol(_methodSymbol, cpIndex, classObject);
   TR::Node *node = TR::Node::createWithSymRef(TR::loadaddr, 0, symRef);
   if (symRef->isUnresolved())
   {
      // We still need to anchor from the stack *as though* we were emitting
      // the ResolveCHK, since the type test will expand to include one later.
      TR::Node *dummyResolveCheck = genResolveCheck(node);
      handleSideEffect(dummyResolveCheck);
      node->decReferenceCount();
      }
   push(node);
   return symRef;
   }

void
TR_J9ByteCodeIlGenerator::loadClassObject(TR_OpaqueClassBlock *opaqueClass)
   {
   loadSymbol(TR::loadaddr, symRefTab()->findOrCreateClassSymbol(_methodSymbol, 0, opaqueClass));
   }

void
TR_J9ByteCodeIlGenerator::loadClassObjectAndIndirect(int32_t cpIndex)
   {
   void * classObject = method()->getClassFromConstantPool(comp(), cpIndex);
   loadSymbol(TR::loadaddr, symRefTab()->findOrCreateClassSymbol(_methodSymbol, cpIndex, classObject));
   TR::Node* node = pop();
   node = TR::Node::createWithSymRef(TR::aloadi, 1, 1, node, symRefTab()->findOrCreateJavaLangClassFromClassSymbolRef());
   push(node);
   }


void
TR_J9ByteCodeIlGenerator::loadConstant(TR::ILOpCodes loadop, int32_t constant)
   {
   push(TR::Node::create(loadop, 0, constant));
   }

void
TR_J9ByteCodeIlGenerator::loadConstant(TR::ILOpCodes loadop, int64_t constant)
   {
   TR::Node * node = TR::Node::create(loadop, 0);
   node->setConstValue(constant);
   push(node);
   }

void
TR_J9ByteCodeIlGenerator::loadConstant(TR::ILOpCodes loadop, float constant)
   {
   TR::Node * node = TR::Node::create(loadop, 0);
   node->setFloat(constant);
   push(node);
   }

void
TR_J9ByteCodeIlGenerator::loadConstant(TR::ILOpCodes loadop, double constant)
   {
   TR::Node * node = TR::Node::create(loadop, 0);
   node->setDouble(constant);
   push(node);
   }

void
TR_J9ByteCodeIlGenerator::loadConstant(TR::ILOpCodes loadop, void * constant)
   {
   TR::Node * node = TR::Node::create(loadop, 0);
   node->setAddress((uintptrj_t)constant);
   push(node);
   }

void
TR_J9ByteCodeIlGenerator::loadFromCP(TR::DataType type, int32_t cpIndex)
   {
   static char *floatInCP = feGetEnv("TR_FloatInCP");
   if (type == TR::NoType)
      type = method()->getLDCType(cpIndex);
   switch (type)
      {
      case TR::Int32:    loadConstant(TR::iconst, (int32_t)method()->intConstant(cpIndex)); break;
      case TR::Int64:    loadConstant(TR::lconst, (int64_t)method()->longConstant(cpIndex)); break;
      case TR::Float:
         if (!floatInCP)
            loadConstant(TR::fconst, * method()->floatConstant(cpIndex));
         else
            loadSymbol(TR::fload, symRefTab()->findOrCreateFloatSymbol(_methodSymbol, cpIndex));
         break;
      case TR::Double:
         if (!floatInCP)
            loadConstant(TR::dconst, *(double*)method()->doubleConstant(cpIndex, trMemory()));
         else
            loadSymbol(TR::dload, symRefTab()->findOrCreateDoubleSymbol(_methodSymbol, cpIndex));
         break;
      case TR::Address:
         if (method()->isClassConstant(cpIndex))
            {
            if (TR::Compiler->cls.classesOnHeap())
               loadClassObjectAndIndirect(cpIndex);
            else
               loadClassObject(cpIndex);
            }
         else if (method()->isStringConstant(cpIndex))
            {
            loadSymbol(TR::aload, symRefTab()->findOrCreateStringSymbol(_methodSymbol, cpIndex));
            }
         else if (method()->isMethodHandleConstant(cpIndex))
            {
            loadSymbol(TR::aload, symRefTab()->findOrCreateMethodHandleSymbol(_methodSymbol, cpIndex));
            }
         else
            {
            TR_ASSERT(method()->isMethodTypeConstant(cpIndex), "Address-type CP entry %d must be class, string, methodHandle, or methodType", cpIndex);
            loadSymbol(TR::aload, symRefTab()->findOrCreateMethodTypeSymbol(_methodSymbol, cpIndex));
            }
         break;
      default:
      	break;
      }
   }

void
TR_J9ByteCodeIlGenerator::loadFromCallSiteTable(int32_t callSiteIndex)
   {
   TR::SymbolReference *symRef = symRefTab()->findOrCreateCallSiteTableEntrySymbol(_methodSymbol, callSiteIndex);
   TR::Node *load = loadSymbol(TR::aload, symRef);
   if (!symRef->isUnresolved())
      {
      if (_methodSymbol->getResolvedMethod()->callSiteTableEntryAddress(callSiteIndex))
         load->setIsNonNull(true);
      else
         load->setIsNull(true);
      }
   }

void
TR_J9ByteCodeIlGenerator::loadFromMethodTypeTable(int32_t cpIndex)
   {
   TR::SymbolReference *symRef = symRefTab()->findOrCreateMethodTypeTableEntrySymbol(_methodSymbol, cpIndex);
   TR::Node *load = loadSymbol(TR::aload, symRef);
   if (!symRef->isUnresolved())
      {
      if (_methodSymbol->getResolvedMethod()->methodTypeTableEntryAddress(cpIndex))
         load->setIsNonNull(true);
      else
         load->setIsNull(true);
      }
   }

void
TR_J9ByteCodeIlGenerator::loadArrayElement(TR::DataType dataType, TR::ILOpCodes nodeop, bool checks)
   {
   bool genSpineChecks = comp()->requiresSpineChecks();

   _suppressSpineChecks = false;

   calculateArrayElementAddress(dataType, checks);

   TR::Node * arrayBaseAddress = pop();
   TR::Node * elementAddress = pop();

   TR::Node *element = NULL;
   TR::SymbolReference * elementSymRef = symRefTab()->findOrCreateArrayShadowSymbolRef(dataType, arrayBaseAddress);
   element = TR::Node::createWithSymRef(nodeop, 1, 1, elementAddress, elementSymRef);

   // For hybrid arrays, an incomplete check node may have been pushed on the stack.
   // It may not exist if the bound and spine check have been skipped.
   //
   TR::Node *checkNode = NULL;

   if (genSpineChecks && !_stack->isEmpty())
      {
      if (_stack->top()->getOpCode().isSpineCheck())
         {
         checkNode = pop();
         }
      }

   if (dataType == TR::Address)
      {

      if (comp()->useCompressedPointers())
         {
         // Returns non-null if the compressedRefs anchor is going to
         // be part of the subtrees
         //
         // We don't want this in a separate treetop for hybrid arraylets.
         //
         TR::Node *newElement = genCompressedRefs(element);
         if (newElement)
            element = newElement;
         }
      }

   if (checkNode)
      {
      if (checkNode->getOpCode().isBndCheck())
         {
         TR_ASSERT(checkNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK, "unexpected check node");

         // Re-arrange children now that load and base address
         // are known.
         //
         checkNode->setChild(2, checkNode->getChild(0));  // arraylength
         checkNode->setChild(3, checkNode->getChild(1));  // index
         }
      else
         {
         TR_ASSERT(checkNode->getOpCodeValue() == TR::SpineCHK, "unexpected check node");
         checkNode->setChild(2, checkNode->getChild(0));  // index
         }

      checkNode->setSpineCheckWithArrayElementChild(true);
      checkNode->setAndIncChild(0, element);              // array element
      checkNode->setAndIncChild(1, arrayBaseAddress);     // base array
      }

   push(element);
   }

void
TR_J9ByteCodeIlGenerator::loadMonitorArg()
   {
   TR_ASSERT(_methodSymbol->isSynchronised(), "loadMonitorArg called for an nonsynchronized method");

   // the syncObjectTemp is always initialized with the monitor argument on entry
   // to the method (regarless of whether its a static sync method or a sync method)
   // we don't want to use the syncObjectTemp always at the monexit because the monent and monexit
   // use different symRefs and this can cause problems for redundant monitor elimination.
   // we use the syncObjectTemp only when the outermost method is a DLT compile
   // for a DLT compile, if the compilation entered directly into a synchronized region, the syncObjectTemp
   // will be refreshed with the correct value in the DLT block
   //
   bool useSyncObjectTemp = comp()->isDLT() && _methodSymbol == comp()->getJittedMethodSymbol();

   if (_methodSymbol->isStatic())
      loadSymbol(TR::loadaddr, symRefTab()->findOrCreateClassSymbol(_methodSymbol, 0, method()->containingClass()));
   else if (useSyncObjectTemp && _methodSymbol->getSyncObjectTemp())
      loadSymbol(TR::aload, _methodSymbol->getSyncObjectTemp());
   else
      loadAuto(TR::Address, 0);     // get this pointer
   }

//----------------------------------------------
// gen monitor
//----------------------------------------------

void
TR_J9ByteCodeIlGenerator::genMonitorEnter()
   {
   TR::SymbolReference * monitorEnterSymbolRef = symRefTab()->findOrCreateMonitorEntrySymbolRef(_methodSymbol);

   TR::Node * node = pop();

   bool isStatic = (node->getOpCodeValue() == TR::loadaddr && node->getSymbol()->isClassObject());

   if (isStatic && TR::Compiler->cls.classesOnHeap())
      node = TR::Node::createWithSymRef(TR::aloadi, 1, 1, node, symRefTab()->findOrCreateJavaLangClassFromClassSymbolRef());

   TR::Node *loadNode = node;
   // We need to keep all synchronized objects on the stack.  We'll create meta data that represents each locked object at each
   // gc point so that the VM can figure out which objects are locked on the stack at any given gc point
   // code moved below, so that the store happens after the monent
   /*
   if (node->getOpCodeValue() != TR::aload || !comp()->getOption(TR_DisableLiveMonitorMetadata))
      {
      TR::SymbolReference * tempSymRef = symRefTab()->createTemporary(_methodSymbol, TR::Address);
      if (!comp()->getOption(TR_DisableLiveMonitorMetadata))
         {
    tempSymRef->getSymbol()->setHoldsMonitoredObject();
    comp()->addMonitorAuto(tempSymRef->getSymbol()->castToRegisterMappedSymbol(), comp()->getCurrentInlinedSiteIndex());
    }
      genTreeTop(TR::Node::createStore(tempSymRef, node));
      node = TR::Node::createLoad(tempSymRef);
      }
   */

   node = TR::Node::createWithSymRef(TR::monent, 1, 1, node, monitorEnterSymbolRef);
   if (isStatic)
      node->setStaticMonitor(true);

   genTreeTop(genNullCheck(node));

   if (!comp()->getOption(TR_DisableLiveMonitorMetadata))
      {
      TR::SymbolReference * tempSymRef = symRefTab()->createTemporary(_methodSymbol, TR::Address);
      comp()->addAsMonitorAuto(tempSymRef, false);
      genTreeTop(TR::Node::createStore(tempSymRef, loadNode));
      }

   _methodSymbol->setMayContainMonitors(true);
   }

void
TR_J9ByteCodeIlGenerator::genMonitorExit(bool isReturn)
   {
   TR::SymbolReference * monitorExitSymbolRef =
      isReturn && isOutermostMethod() ?
         symRefTab()->findOrCreateMethodMonitorExitSymbolRef(_methodSymbol) :
         symRefTab()->findOrCreateMonitorExitSymbolRef(_methodSymbol);

   TR::Node * node = pop();

   bool isStatic = (node->getOpCodeValue() == TR::loadaddr && node->getSymbol()->isClassObject());
   ///bool isStatic = _methodSymbol->isStatic();

   if (isStatic && TR::Compiler->cls.classesOnHeap())
      node = TR::Node::createWithSymRef(TR::aloadi, 1, 1, node, symRefTab()->findOrCreateJavaLangClassFromClassSymbolRef());

   if (!comp()->getOption(TR_DisableLiveMonitorMetadata))
      {
      genTreeTop(TR::Node::create(TR::monexitfence,0));
      }
   node = TR::Node::createWithSymRef(TR::monexit, 1, 1, node, monitorExitSymbolRef);

   if (isReturn)
      {
      if (_methodSymbol->isStatic())
         node->setStaticMonitor(true);

      node->setSyncMethodMonitor(true);

      TR_OpaqueClassBlock * owningClass = _methodSymbol->getResolvedMethod()->containingClass();
      if (owningClass != comp()->getObjectClassPointer())
        node->setSecond((TR::Node*)owningClass);

      _implicitMonitorExits.add(node);
      }

   node = genNullCheck(node);

   // The monitor exit will unlock the class object which will allow other threads to modify the
   // fields in this class.  To ensure that any fields referenced by the return are evaluated
   // before the monitor exit call the shadows symbols on the stack must be anchored before the
   // monitor exit call.
   //
   handleSideEffect(node);

   genTreeTop(node);
   _methodSymbol->setMayContainMonitors(true);
   }

//----------------------------------------------
// gen new
//----------------------------------------------

void
TR_J9ByteCodeIlGenerator::genNew(int32_t cpIndex)
   {
   loadClassObject(cpIndex);
   genNew();
   }

void
TR_J9ByteCodeIlGenerator::genNew(TR::ILOpCodes opCode)
   {
   TR::Node *node = TR::Node::createWithSymRef(opCode, 1, 1, pop(),symRefTab()->findOrCreateNewObjectSymbolRef(_methodSymbol));
   _methodSymbol->setHasNews(true);
   genTreeTop(node);
   push(node);

   bool skipFlush = false;
   if (!node->getFirstChild()->getSymbolReference()->isUnresolved() && node->getFirstChild()->getSymbol()->isStatic())
      {
      TR_OpaqueClassBlock *clazz = (TR_OpaqueClassBlock*)node->getFirstChild()->getSymbol()->castToStaticSymbol()->getStaticAddress();
      int32_t len;
      char *sig;
      sig = TR::Compiler->cls.classSignature_DEPRECATED(comp(), clazz, len, comp()->trMemory());
      OMR::ResolvedMethodSymbol *resolvedMethodSymbol = node->getSymbol()->getResolvedMethodSymbol();

      if (((len == 16) && strncmp(sig, "Ljava/lang/Long;", 16) == 0) ||
          ((len == 16) && strncmp(sig, "Ljava/lang/Byte;", 16) == 0) ||
          ((len == 17) && strncmp(sig, "Ljava/lang/Short;", 17) == 0) ||
          ((len == 18) && strncmp(sig, "Ljava/lang/String;", 18) == 0) ||
          ((len == 19) && strncmp(sig, "Ljava/lang/Integer;", 19) == 0) ||
          ((len == 19) && strncmp(sig, "Ljava/util/HashMap;", 19) == 0) ||
          ((len == 21) && strncmp(sig, "Ljava/lang/Character;", 21) == 0) ||
          ((len == 21) && strncmp(sig, "Ljava/nio/CharBuffer;", 21) == 0) ||
          ((len == 21) && strncmp(sig, "Ljava/nio/ByteBuffer;", 21) == 0) ||
          ((len == 24) && strncmp(sig, "Ljava/util/HashMap$Node;", 24) == 0) ||
          ((len == 25) && strncmp(sig, "Ljava/util/ArrayList$Itr;", 25) == 0) ||
          ((len == 25) && strncmp(sig, "Ljava/nio/HeapCharBuffer;", 25) == 0) ||
          ((len == 25) && strncmp(sig, "Ljava/nio/HeapByteBuffer;", 25) == 0) ||
          ((len == 25) && strncmp(sig, "Ljava/util/LinkedHashMap;", 25) == 0) ||
          ((len == 26) && strncmp(sig, "Ljava/util/HashMap$KeySet;", 26) == 0) ||
          ((len == 27) && strncmp(sig, "Ljava/util/Hashtable$Entry;", 27) == 0) ||
          ((len == 28) && strncmp(sig, "Ljava/util/AbstractList$Itr;", 28) == 0) ||
          ((len == 28) && strncmp(sig, "Ljava/util/HashMap$EntrySet;", 28) == 0) ||
          ((len == 30) && strncmp(sig, "Ljava/util/LinkedList$ListItr;", 30) == 0) ||
          ((len == 31) && strncmp(sig, "Ljava/util/HashMap$KeyIterator;", 31) == 0) ||
          ((len == 32) && strncmp(sig, "Ljava/util/HashMap$HashIterator;", 32) == 0) ||
          ((len == 33) && strncmp(sig, "Ljava/util/HashMap$ValueIterator;", 33) == 0) ||
          ((len == 33) && strncmp(sig, "Ljava/util/HashMap$EntryIterator;", 33) == 0) ||
          ((len == 33) && strncmp(sig, "Ljava/nio/charset/CharsetDecoder;", 33) == 0) ||
          ((len == 35) && strncmp(sig, "Ljavax/servlet/ServletRequestEvent;", 35) == 0) ||
          ((len == 44) && strncmp(sig, "Ljavax/servlet/ServletRequestAttributeEvent;", 44) == 0) ||
          ((len == 45) && strncmp(sig, "Ljava/util/concurrent/ConcurrentHashMap$Node;", 45) == 0) ||
          ((len == 53) && strncmp(sig, "Ljavax/faces/component/_DeltaStateHelper$InternalMap;", 53) == 0) ||
          ((len == 55) && strncmp(sig, "Ljava/util/concurrent/CopyOnWriteArrayList$COWIterator;", 55) == 0) ||
          ((len == 68) && strncmp(sig, "Ljava/util/concurrent/locks/ReentrantReadWriteLock$Sync$HoldCounter;", 68) == 0) ||
          ((len == 25) && strncmp(sig, "Ljava/util/PriorityQueue;", 25) == 0) ||
          ((len == 42) && strncmp(sig, "Ljava/util/concurrent/locks/ReentrantLock;", 42) == 0) ||
          ((len == 54) && strncmp(sig, "Ljava/util/concurrent/locks/ReentrantLock$NonfairSync;", 54) == 0) 
         )
         {
         skipFlush = true;
         }
      }

      if (!skipFlush)
         genFlush(0);
   }

void
TR_J9ByteCodeIlGenerator::genNewArray(int32_t typeIndex)
   {
   loadConstant(TR::iconst, typeIndex);

   TR::Node * secondChild=pop();
   TR::Node * firstChild=pop();
   TR::Node * node = TR::Node::createWithSymRef(TR::newarray, 2, 2, firstChild, secondChild, symRefTab()->findOrCreateNewArraySymbolRef(_methodSymbol));

   if (_methodSymbol->skipZeroInitializationOnNewarrays())
     node->setCanSkipZeroInitialization(true);

   // special case for handling Arrays.copyOf in the StringEncoder fast paths for Java 9+
   if (!comp()->isOutermostMethod()
       && _methodSymbol->getRecognizedMethod() == TR::java_util_Arrays_copyOf_byte)
     {
     int32_t callerIndex = comp()->getCurrentInlinedCallSite()->_byteCodeInfo.getCallerIndex();
     TR::ResolvedMethodSymbol *caller = callerIndex > -1 ? comp()->getInlinedResolvedMethodSymbol(callerIndex) : comp()->getMethodSymbol();
     switch (caller->getRecognizedMethod())
        {
        case TR::java_lang_StringCoding_encode8859_1:
        case TR::java_lang_StringCoding_encodeASCII:
        case TR::java_lang_StringCoding_encodeUTF8:
           node->setCanSkipZeroInitialization(true);
           break;
        }
     }

   bool separateInitializationFromAllocation;
   switch (_methodSymbol->getRecognizedMethod())
      {
      case TR::java_util_Arrays_copyOf_byte:
      case TR::java_util_Arrays_copyOf_short:
      case TR::java_util_Arrays_copyOf_char:
      case TR::java_util_Arrays_copyOf_int:
      case TR::java_util_Arrays_copyOf_long:
      case TR::java_util_Arrays_copyOf_float:
      case TR::java_util_Arrays_copyOf_double:
      case TR::java_util_Arrays_copyOf_boolean:
      case TR::java_util_Arrays_copyOfRange_byte:
      case TR::java_util_Arrays_copyOfRange_short:
      case TR::java_util_Arrays_copyOfRange_char:
      case TR::java_util_Arrays_copyOfRange_int:
      case TR::java_util_Arrays_copyOfRange_long:
      case TR::java_util_Arrays_copyOfRange_float:
      case TR::java_util_Arrays_copyOfRange_double:
      case TR::java_util_Arrays_copyOfRange_boolean:
         separateInitializationFromAllocation = true;
         break;
      default:
         separateInitializationFromAllocation = false;
         break;
      }

   TR::Node *initNode = NULL;

   bool generateArraylets = comp()->generateArraylets();

   if (!comp()->getOption(TR_DisableSeparateInitFromAlloc) &&
       !node->canSkipZeroInitialization() &&
       !generateArraylets && separateInitializationFromAllocation &&
       (comp()->cg()->getSupportsArraySet() || comp()->cg()->getSupportsArraySetToZero()))
      {
      node->setCanSkipZeroInitialization(true);

      TR::Node *arrayRefNode;
      int32_t hdrSize = (int32_t) TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
      bool is64BitTarget = TR::Compiler->target.is64Bit();

      if (is64BitTarget)
         {
         TR::Node *constantNode = TR::Node::create(node, TR::lconst);
         constantNode->setLongInt((int64_t)hdrSize);
         arrayRefNode = TR::Node::create(TR::aladd, 2, node, constantNode);
         }
      else
         arrayRefNode = TR::Node::create(TR::aiadd, 2, node, TR::Node::create(node, TR::iconst, 0, hdrSize));

      arrayRefNode->setIsInternalPointer(true);

      TR::Node *sizeInBytes;
      TR::Node *sizeNode = node->getFirstChild();

      TR::Node* constValNode = TR::Node::bconst(node, (int8_t)0);
      int32_t elementSize = TR::Compiler->om.getSizeOfArrayElement(node);

      if (is64BitTarget)
         {
         TR::Node *stride = TR::Node::create(node, TR::lconst);
         stride->setLongInt((int64_t) elementSize);
         TR::Node *i2lNode = TR::Node::create(TR::i2l, 1, sizeNode);
         sizeInBytes = TR::Node::create(TR::lmul, 2, i2lNode, stride);
         }
      else
         {
         TR::Node *stride = TR::Node::create(node, TR::iconst, 0, elementSize);
         sizeInBytes = TR::Node::create(TR::imul, 2, sizeNode, stride);
         }

      TR::Node *arraysetNode = TR::Node::create(TR::arrayset, 3, arrayRefNode, constValNode, sizeInBytes);
      TR::SymbolReference *arraySetSymRef = comp()->getSymRefTab()->findOrCreateArraySetSymbol();
      arraysetNode->setSymbolReference(arraySetSymRef);
      arraysetNode->setArraysetLengthMultipleOfPointerSize(true);

      initNode = TR::Node::create(TR::treetop, 1, arraysetNode);
      }

   _methodSymbol->setHasNews(true);
   genTreeTop(node);
   if (initNode)
      genTreeTop(initNode);
   push(node);
   genFlush(0);
   }

void
TR_J9ByteCodeIlGenerator::genANewArray(int32_t cpIndex)
   {
   loadClassObject(cpIndex);
   genANewArray();
   }

void
TR_J9ByteCodeIlGenerator::genANewArray()
   {
   TR::Node * secondChild=pop();
   TR::Node * firstChild=pop();
   TR::Node * node = TR::Node::createWithSymRef(TR::anewarray, 2, 2, firstChild, secondChild, symRefTab()->findOrCreateANewArraySymbolRef(_methodSymbol));
   _methodSymbol->setHasNews(true);
   genTreeTop(node);
   push(node);
   genFlush(0);
   }

void
TR_J9ByteCodeIlGenerator::genMultiANewArray(int32_t cpIndex, int32_t dims)
   {
   // total number of params is 2+#dims
   //
   loadClassObject(cpIndex);
   genMultiANewArray(dims);
   }

void
TR_J9ByteCodeIlGenerator::genMultiANewArray(int32_t dims)
   {
   // total number of params is 2+#dims
   //
   TR::Node *node = genNodeAndPopChildren(TR::multianewarray, dims+2, symRefTab()->findOrCreateMultiANewArraySymbolRef(_methodSymbol), 1);

   _methodSymbol->setHasNews(true);
   // Make number of dimensions the first parameter
   //
   loadConstant(TR::iconst, dims);
   node->setAndIncChild(0, pop());

   genTreeTop(node);
   push(node);
   }

//----------------------------------------------
// genReturn
//----------------------------------------------

int32_t
TR_J9ByteCodeIlGenerator::genReturn(TR::ILOpCodes nodeop, bool monitorExit)
   {
   if (!comp()->isPeekingMethod() &&
         (_methodSymbol->getMandatoryRecognizedMethod() == TR::java_lang_Object_init))
      {
      TR::Node *receiverArg = NULL;
      if (_methodSymbol->getThisTempForObjectCtor())
         receiverArg = TR::Node::createLoad(_methodSymbol->getThisTempForObjectCtor());
      else
         {
         loadAuto(TR::Address, 0);
         receiverArg = pop();
         }
      TR::SymbolReference *finalizeSymRef = comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_jitCheckIfFinalizeObject, true, true, true);
      TR::Node *vcallNode = TR::Node::createWithSymRef(TR::call, 1, 1, receiverArg, finalizeSymRef);
      _finalizeCallsBeforeReturns.add(vcallNode);
      genTreeTop(vcallNode);
      }

   static const char* disableMethodHookForCallees = feGetEnv("TR_DisableMethodHookForCallees");
   if ((fej9()->isMethodExitTracingEnabled(_methodSymbol->getResolvedMethod()->getPersistentIdentifier()) ||
        TR::Compiler->vm.canMethodExitEventBeHooked(comp()))
         && (isOutermostMethod() || !disableMethodHookForCallees)) 
      {
      TR::SymbolReference * methodExitSymRef = symRefTab()->findOrCreateReportMethodExitSymbolRef(_methodSymbol);

      TR::Node * methodExitNode;
      if (comp()->getOption(TR_OldJVMPI))
         methodExitNode = TR::Node::createWithSymRef(TR::MethodExitHook, 0, methodExitSymRef);
      else if (nodeop == TR::Return)
         {
         loadConstant(TR::aconst, (void *)0);
         methodExitNode = TR::Node::createWithSymRef(TR::MethodExitHook, 1, 1, pop(), methodExitSymRef);
         }
      else
         {
         TR::Node * returnValue = _stack->top();
         TR::SymbolReference * tempSymRef = symRefTab()->createTemporary(_methodSymbol, getDataType(returnValue));
         genTreeTop(TR::Node::createStore(tempSymRef, returnValue));
         methodExitNode = TR::Node::createWithSymRef(TR::MethodExitHook, 1, 1, TR::Node::createWithSymRef(TR::loadaddr, 0, tempSymRef), methodExitSymRef);
         }

      genTreeTop(methodExitNode);
      }


   if (comp()->getOption(TR_EnableThisLiveRangeExtension))
      {
      if (!_methodSymbol->isStatic() &&
          (!fej9()->isClassFinal(_methodSymbol->getResolvedMethod()->containingClass()) ||
           fej9()->hasFinalizer(_methodSymbol->getResolvedMethod()->containingClass())))
         {
         loadAuto(TR::Address, 0);
         TR::SymbolReference *tempSymRef = symRefTab()->findOrCreateThisRangeExtensionSymRef(comp()->getMethodSymbol());
         genTreeTop(TR::Node::createStore(tempSymRef, pop()));
         }
      }

   if (monitorExit && _methodSymbol->isSynchronised())
      {
      if (!isOutermostMethod())
         {
         // the monitor exit in an inlined method must be in a separate block so that the generated exception range
         // doesn't include it.
         //
         genTarget(_bcIndex);
         setupBBStartContext(_bcIndex);
         //printf("create a separate block for %s being inlined into %s\n", _methodSymbol->signature(trMemory()), comp()->signature());
         }

      loadMonitorArg();
      genMonitorExit(true);
      }

   if (nodeop == TR::Return)
      genTreeTop(TR::Node::create(nodeop, 0));
   else
      genTreeTop(TR::Node::create(nodeop, 1, pop()));

   discardEntireStack();

   return findNextByteCodeToGen();
   }

static bool storeCanBeRemovedForUnreadField(TR_PersistentFieldInfo * fieldInfo, TR::Node *node)
   {
   if (!fieldInfo ||
       !fieldInfo->isNotRead())
      return false;

   // PR 78765: It's generally not safe to remove field stores because we can't
   // prove that native methods won't read them.  It's also not safe to remove
   // address field stores in case the stored object has a finalizer; failing
   // to store the reference can cause the finalizer to run prematurely.
   //
   // However, in certain cases, this is known to be safe, so we can detect
   // those and optimize away the stores.

   if (node->getOpCode().isCall() &&
       !node->getSymbolReference()->isUnresolved())
      {
      if (fieldInfo->isBigDecimalType())
         {
         if ((node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_add) ||
             (node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_subtract) ||
             (node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigDecimal_multiply))
            return true;
         }

      if (fieldInfo->isBigIntegerType())
         {
         if ((node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigInteger_add) ||
             (node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigInteger_subtract) ||
             (node->getSymbol()->getResolvedMethodSymbol()->getRecognizedMethod() == TR::java_math_BigInteger_multiply))
            return true;
         }
      }

   return false;
   }

//----------------------------------------------
// gen store
//----------------------------------------------
void
TR_J9ByteCodeIlGenerator::storeInstance(int32_t cpIndex)
   {
   TR::SymbolReference * symRef = symRefTab()->findOrCreateShadowSymbol(_methodSymbol, cpIndex, true);
   TR::Symbol * symbol = symRef->getSymbol();
   TR::DataType type = symbol->getDataType();

   TR::Node * value = pop();
   TR::Node * address = pop();

   TR::Node * addressNode  = address;
   TR::Node * parentObject = address;

   // code to handle volatiles moved to CodeGenPrep
   //
   TR::Node * node;
   if (type == TR::Address && _generateWriteBarriers)
      {
      node = TR::Node::createWithSymRef(TR::wrtbari, 3, 3, addressNode, value, parentObject, symRef);
      }
   else
      node = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectStore(type), 2, 2, addressNode, value, symRef);

   if (symbol->isPrivate() && _classInfo && comp()->getNeedsClassLookahead())
      {
      if (!_classInfo->getFieldInfo())
         performClassLookahead(_classInfo);

      TR_PersistentFieldInfo * fieldInfo = _classInfo->getFieldInfo() ? _classInfo->getFieldInfo()->findFieldInfo(comp(), node, true) : NULL;
      if (storeCanBeRemovedForUnreadField(fieldInfo, value) &&
          performTransformation(comp(), "O^O CLASS LOOKAHEAD: Can skip store to instance field (that is never read) storing value %p based on class file examination\n", value))
         {
         //int32_t length;
         //char *sig = TR_ClassLookahead::getFieldSignature(comp(), symbol, symRef, length);
         //fprintf(stderr, "Skipping store for field %s in %s\n", sig, comp()->signature());
 //fflush(stderr);
         genTreeTop(value);
         genTreeTop(address);
         int32_t numChildren = node->getNumChildren();
         int32_t i = 0;
         while (i < numChildren)
            {
            node->getChild(i)->decReferenceCount();
            i++;
            }

         if (!address->isNonNull())
            {
            TR::Node *passThruNode = TR::Node::create(TR::PassThrough, 1, address);
            genTreeTop(genNullCheck(passThruNode));
            }

         return;
         }
      }
   if (symbol->isPrivate() && !comp()->getOptions()->realTimeGC())
      {
      TR_ResolvedMethod  *method;

      if (node->getInlinedSiteIndex() != -1)
         method = comp()->getInlinedResolvedMethod(node->getInlinedSiteIndex());
      else
         method = comp()->getCurrentMethod();

      if (method &&  method->getRecognizedMethod() == TR::java_lang_ref_SoftReference_get &&
          symbol->getRecognizedField() == TR::Symbol::Java_lang_ref_SoftReference_age)
         {
         TR::Node *secondChild = node->getChild(1);
         if (secondChild && secondChild->getOpCodeValue() == TR::iconst && secondChild->getInt() == 0)
            {
            symbol->resetVolatile();
            handleSideEffect(node);
            genTreeTop(node);
            genFullFence(node);
            return;
            }
         }
      }
   bool genTranslateTT = (comp()->useCompressedPointers() && (type == TR::Address));

   if (symRef->isUnresolved())
      {
      if (!address->isNonNull())
         node = genResolveAndNullCheck(node);
      else
         node = genResolveCheck(node);

      genTranslateTT = false;
      }
   else if (!address->isNonNull())
      {
      TR::Node *nullChkNode = genNullCheck(node);
      // in some cases a nullchk may not
      // have been generated at all for the store
      //
      if (nullChkNode != node)
         genTranslateTT = false;
      node = nullChkNode;
      }

   handleSideEffect(node);

   if (!genTranslateTT)
      genTreeTop(node);

   if (comp()->useCompressedPointers() &&
         (type == TR::Address))
      {
      // - J9JIT_COMPRESSED_POINTER J9CLASS HACK-
      // remove this check when j9class is allocated on the heap
      // do not compress fields that contain class pointers
      //
      TR::Node *storeValue = node;
      if (storeValue->getOpCode().isCheck())
         storeValue = storeValue->getFirstChild();

      if (!symRefTab()->isFieldClassObject(symRef))
         {
         // returns non-null if the compressedRefs anchor is going to
         // be part of the subtrees (for now, it is a treetop)
         //
         TR::Node *newValue = genCompressedRefs(storeValue, true, -1);
         if (newValue)
            {
            node->getSecondChild()->decReferenceCount();
            node->setAndIncChild(1, newValue);
            }
         }
      else
         genTreeTop(node);
      }
   }

void
TR_J9ByteCodeIlGenerator::storeStatic(int32_t cpIndex)
   {
   _staticFieldReferenceEncountered = true;
   TR::Node * value = pop();

   TR::SymbolReference * symRef = symRefTab()->findOrCreateStaticSymbol(_methodSymbol, cpIndex, true);
   TR::StaticSymbol * symbol = symRef->getSymbol()->castToStaticSymbol();

   TR::DataType type = symbol->getDataType();

   TR::Node * node;

   if (type == TR::Address && _generateWriteBarriers)
      {
      void * staticClass = method()->classOfStatic(cpIndex);
      loadSymbol(TR::loadaddr, symRefTab()->findOrCreateClassSymbol(_methodSymbol, cpIndex, staticClass, true));

      if (TR::Compiler->cls.classesOnHeap())
         {
         node = pop();
         node = TR::Node::createWithSymRef(TR::aloadi, 1, 1, node, symRefTab()->findOrCreateJavaLangClassFromClassSymbolRef());
         push(node);
         }

      node = TR::Node::createWithSymRef(TR::wrtbar, 2, 2, value, pop(), symRef);
      }
   else if (symbol->isVolatile() && type == TR::Int64 && !symRef->isUnresolved() && TR::Compiler->target.is32Bit() &&
            !comp()->cg()->getSupportsInlinedAtomicLongVolatiles() && 0)
      {
      TR::SymbolReference *volatileLongSymRef =
         comp()->getSymRefTab()->findOrCreateRuntimeHelper (TR_volatileWriteLong, false, false, true);
      TR::Node * statics = TR::Node::createWithSymRef(TR::loadaddr, 0, symRef);

      node = TR::Node::createWithSymRef(TR::call, 2, 2, value, statics, volatileLongSymRef);
      }
   else if (!symRef->isUnresolved() && cg()->getAccessStaticsIndirectly() && type != TR::Address && !comp()->compileRelocatableCode())
      {
      TR::Node * statics = TR::Node::createWithSymRef(TR::loadaddr, 0, symRefTab()->findOrCreateClassStaticsSymbol(_methodSymbol, cpIndex));
      node = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectStore(type), 2, 2, statics, value, symRef);
      }
   else
      {
      node = TR::Node::createStore(symRef, value);
      }

   if (symbol->isPrivate() && _classInfo && comp()->getNeedsClassLookahead() && !symbol->isVolatile())
      {
      if (!_classInfo->getFieldInfo())
         performClassLookahead(_classInfo);

      // findFieldInfo will update node, if node is array shadow, as it set canBeArrayShadow=true
      // For normal static findFieldInfo will not update node, it can't be arrayShadown store
      // So set canBeArrayShadow false here
      //
      TR_PersistentFieldInfo * fieldInfo = _classInfo->getFieldInfo() ? _classInfo->getFieldInfo()->findFieldInfo(comp(), node, false) : NULL;
      //traceMsg(comp(), "Field %p info %p\n", node, fieldInfo);
      if (storeCanBeRemovedForUnreadField(fieldInfo, value) &&
          performTransformation(comp(), "O^O CLASS LOOKAHEAD: Can skip store to static (that is never read) storing value %p based on class file examination\n", value))
         {
         //int32_t length;
         //char *sig = TR_ClassLookahead::getFieldSignature(comp(), symbol, symRef, length);
         //fprintf(stderr, "Skipping store for field %s in %s\n", sig, comp()->signature());
 //fflush(stderr);
         int32_t numChildren = node->getNumChildren();
         int32_t i = 0;
         while (i < numChildren)
            {
            genTreeTop(node->getChild(i));
            node->getChild(i)->decReferenceCount();
            i++;
            }
         return;
         }
      }

   if (symRef->isUnresolved())
      node = genResolveCheck(node);

   handleSideEffect(node);

   genTreeTop(node);
   }

void
TR_J9ByteCodeIlGenerator::storeDualAuto(TR::Node * storeValue, int32_t slot)
   {
   TR_ASSERT(storeValue->isDualHigh() || storeValue->isTernaryHigh(), "Coerced types only happen when a dual or ternary operator is generated.");

   // type may need to be coerced from TR::Address into the type of the value being stored
   TR::DataType type = storeValue->getDataType();

   // generate the two stores for the storeValue and its adjunct.
   TR::Node* adjunctValue = storeValue->getChild(2);
   if (storeValue->isTernaryHigh())
      {
      adjunctValue = adjunctValue->getFirstChild();
      }
   push(storeValue);
   storeAuto(type, slot);
   push(adjunctValue);
   storeAuto(type, slot, true);
   return;
   }

void
TR_J9ByteCodeIlGenerator::storeAuto(TR::DataType type, int32_t slot, bool isAdjunct)
   {
   TR::Node* storeValue = pop();

   TR::SymbolReference * symRef;

   if (storeValue->getDataType() != type && type == TR::Address)
      {
      // this presently happens only when an operator returning Quad was coerced from
      // a TR::Address type into a TR_SInt64 type, and a "dual operator" was created.
      // store the dual symbol
      storeDualAuto(storeValue, slot);
      return;
      }

   symRef = symRefTab()->findOrCreateAutoSymbol(_methodSymbol, slot, type, true, false, true, isAdjunct);
   if (storeValue->isDualHigh() || storeValue->isTernaryHigh() || isAdjunct)
      symRef->setIsDual();

   bool isStatic = _methodSymbol->isStatic();

   //Partial Inlining: if we store into a parameter we need to create a temporary for the callback.  We need to create the store for the temp in the first block.

   int32_t numParmSlots =  _methodSymbol->getNumParameterSlots();

   if(_blocksToInline)
      {
      if (slot < numParmSlots)
         {
         //printf("Walker: (partial)storeAuto: storing into a Parameter. numParmSlots = %d isStatic = %d slot = %d\n",numParmSlots,isStatic,slot);
         //Need to create a temporary and use it in the callback
         TR::Block *firstBlock = blocks(0);

         TR::SymbolReference *tempRef = symRefTab()->createTemporary(_methodSymbol,type);
         TR::Node *loadNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectLoad(type),0,symRef);
         TR::Node *ttNode = TR::Node::createStore(tempRef, loadNode);
         blocks(0)->prepend(TR::TreeTop::create(_compilation, ttNode));

         _blocksToInline->hasGeneratedRestartTree() ? patchPartialInliningCallBack(slot,symRef,tempRef,_blocksToInline->getGeneratedRestartTree()) : addTempForPartialInliningCallBack(slot,tempRef,numParmSlots);
         }
      }
   // If there's a store into the receiver of a synchronized method then we
   // would need to go to some effort to make sure we unlock the right object
   // on exit, and it's just not worth it because nobody actually does this.
   //
   if (slot == 0 && _methodSymbol->isSynchronised() && !isStatic)
      {
      comp()->failCompilation<TR::ILGenFailure>("store to this in sync method");
      }

   TR::Node * node = TR::Node::createStore(symRef, storeValue);

   handleSideEffect(node);

   genTreeTop(node);

   // If there's a store into the receiver of a synchronized method then we have to save the receive into a
   // temp and use the temporary on the monitorexit
   //
   if (slot == 0 && _methodSymbol->isSynchronised() && !isStatic && !_methodSymbol->getSyncObjectTemp()) // RTSJ support
      {
      _methodSymbol->setSyncObjectTemp(symRefTab()->createTemporary(_methodSymbol, TR::Address));
      ListIterator<TR::Node> i(&_implicitMonitorExits);
      for (TR::Node * monexit = i.getFirst(); monexit; monexit = i.getNext())
         {
         TR::Node *newLoad = TR::Node::createLoad(_methodSymbol->getSyncObjectTemp());
         monexit->setChild(0, newLoad);
         }
      }

   // If there's a store into the receiver of Object.<init> then save the receiver into a temporary and use
   // the temporary as the parameter of the call to jitCheckIfFinalize
   //
   if (slot == 0 &&
          _methodSymbol->getResolvedMethod()->isObjectConstructor() &&
          !_methodSymbol->getThisTempForObjectCtor())
      {
      TR::SymbolReference *tempSymRef = symRefTab()->createTemporary(_methodSymbol, TR::Address);
      ///traceMsg(comp(), "created tempSymRef = %d\n", tempSymRef->getReferenceNumber());
      _methodSymbol->setThisTempForObjectCtor(tempSymRef);
      // iterate through other returns in this method to make sure all the calls to jitCheckIfFinalizeObject
      // use this particular symref
      //
      ListIterator<TR::Node> i(&_finalizeCallsBeforeReturns);
      for (TR::Node *finalize = i.getFirst(); finalize; finalize = i.getNext())
         {
         TR::Node *receiverArg = finalize->getFirstChild();
         if (receiverArg->getOpCode().hasSymbolReference() &&
               receiverArg->getSymbolReference() != tempSymRef)
            {
            receiverArg->decReferenceCount();
            receiverArg = TR::Node::createLoad(tempSymRef);
            finalize->setAndIncChild(0, receiverArg);
            }
         }
      }
   }

void
TR_J9ByteCodeIlGenerator::storeArrayElement(TR::DataType dataType, TR::ILOpCodes nodeop, bool checks)
   {
   TR::Node * value = pop();

   handlePendingPushSaveSideEffects(value);

   bool genSpineChecks = comp()->requiresSpineChecks();

   _suppressSpineChecks = false;

   calculateArrayElementAddress(dataType, true);

   TR::Node * arrayBaseAddress = pop();
   bool usedArrayBaseAddress = false;
   TR::Node * elementAddress = pop();

   TR::SymbolReference * symRef = symRefTab()->findOrCreateArrayShadowSymbolRef(dataType, NULL);
   bool generateWriteBarrier = (dataType == TR::Address);

   TR::Node * storeNode, * resultNode;
   if (generateWriteBarrier)
      {
      storeNode = resultNode = TR::Node::createWithSymRef(TR::wrtbari, 3, 3, elementAddress, value, arrayBaseAddress, symRef);
      usedArrayBaseAddress = true;
      }
   else
      {
      storeNode = resultNode = TR::Node::createWithSymRef(nodeop, 2, 2, elementAddress, value, symRef);
      }

   // For hybrid arrays, an incomplete check node may have been pushed on the stack.
   // It may not exist if the bound and spine check have been skipped.
   //
   TR::Node *checkNode = NULL;

   if (genSpineChecks && !_stack->isEmpty())
      {
      if (_stack->top()->getOpCode().isSpineCheck())
         {
         checkNode = pop();
         usedArrayBaseAddress = true;
         }
      }

   if (dataType == TR::Address)
      {
      bool canSkipThisArrayStoreCheck = _methodSymbol->skipArrayStoreChecks() && checks; // transformed java/lang/unsafe calls don't require checks;
      if (!canSkipThisArrayStoreCheck && _classInfo && value->getOpCodeValue() == TR::New)
         {
         if (!_classInfo->getFieldInfo())
            performClassLookahead(_classInfo);

         TR_PersistentFieldInfo * fieldInfo = _classInfo->getFieldInfo() ? _classInfo->getFieldInfo()->findFieldInfo(comp(), arrayBaseAddress, false) : NULL;
         TR_PersistentFieldInfo * arrayFieldInfo = fieldInfo ? fieldInfo->asPersistentArrayFieldInfo() : 0;
         if (arrayFieldInfo && arrayFieldInfo->isTypeInfoValid())
            {
            int32_t len;
            const char * s = value->getFirstChild()->getSymbolReference()->getTypeSignature(len);
            if (arrayFieldInfo->getNumChars() == len && !memcmp(s, arrayFieldInfo->getClassPointer(), len))
               {
               if (performTransformation(comp(), "O^O CLASS LOOKAHEAD: Can skip array store check for value %p using array object %p which has type %s based on class file examination\n", value, arrayBaseAddress, s))
                  canSkipThisArrayStoreCheck = true;
               }
            }
         }

      if (!canSkipThisArrayStoreCheck)
         {
         symRef = symRefTab()->findOrCreateTypeCheckArrayStoreSymbolRef(_methodSymbol);
         TR_ASSERT(generateWriteBarrier,"TR::ArrayStoreCHK needs write barrier support.");
         if (generateWriteBarrier)
            resultNode = TR::Node::createWithRoomForThree(TR::ArrayStoreCHK, storeNode, 0, symRef);
         }
      }

   if (!usedArrayBaseAddress)
      removeIfNotOnStack(arrayBaseAddress);

   handleSideEffect(storeNode);

   // if a compressedRefs anchor has to be
   // generated, dont do anything for if
   // the result is just the store
   //
   bool genTranslateTT = (comp()->useCompressedPointers() && (dataType == TR::Address));

   // If the store is hung off the bound or spine check then do not anchor it
   // under its own treetop.
   //
   if (checkNode)
      {
      if (resultNode->getOpCodeValue() == TR::ArrayStoreCHK || (storeNode->getOpCode().isWrtBar() && !genTranslateTT))
         {
         // Anchor the array store check or the write barrier under its own treetop.
         //
         genTreeTop(resultNode);
         }
      }
   else
      {
      if (!((genTranslateTT) && resultNode->getOpCode().isStore()))
         genTreeTop(resultNode);
      }

   if (genTranslateTT)
      {
      TR::Node *newValue = genCompressedRefs(storeNode, true, -1);
      if (newValue)
         {
         storeNode->getSecondChild()->decReferenceCount();
         storeNode->setAndIncChild(1, newValue);
         }
      }

   if (checkNode)
      {
      if (checkNode->getOpCode().isBndCheck())
         {
         TR_ASSERT(checkNode->getOpCodeValue() == TR::BNDCHKwithSpineCHK, "unexpected check node");

         // Re-arrange children now that element address and base address
         // are known.
         //
         checkNode->setChild(2, checkNode->getChild(0));  // arraylength
         checkNode->setChild(3, checkNode->getChild(1));  // index
         }
      else
         {
         TR_ASSERT(checkNode->getOpCodeValue() == TR::SpineCHK, "unexpected check node");
         checkNode->setChild(2, checkNode->getChild(0));  // index
         }

      // The store node cannot, in general, be hung from the check node
      // because the resulting tree may have multiple side effects.  For example,
      // a BNDCHKwithSpineCHK node with an ArrayStoreCHK beneath it.  So,
      // the tree is created with the array element address instead.  This
      // is sub-optimal in the sense that the destination address is always
      // evaluated into a register first, thereby bypassing any direct memory
      // opportunities.
      //

      if (storeNode->getOpCode().isWrtBar())
         checkNode->setAndIncChild(0, elementAddress);    // iwrtbar
      else
         {
         checkNode->setSpineCheckWithArrayElementChild(true);
         checkNode->setAndIncChild(0, storeNode);         // primitive store
         }
      checkNode->setAndIncChild(1, arrayBaseAddress);     // base array
      }

   }

//----------------------------------------------
// gen switch
//----------------------------------------------

int32_t
TR_J9ByteCodeIlGenerator::genLookupSwitch()
   {
   int32_t i = 1;
   while ((intptrj_t)&_code[_bcIndex+i] & 3) ++i; // 4 byte align

   int32_t bcIndex = _bcIndex + i;
   int32_t defaultTarget = nextSwitchValue(bcIndex) + _bcIndex;
   int32_t tableSize = nextSwitchValue(bcIndex);

   TR::Node * first = pop();

   if (tableSize == 0)
      {
      first->incReferenceCount();
      first->recursivelyDecReferenceCount();
      return genGoto(defaultTarget);
      }

   handlePendingPushSaveSideEffects(first);

   bool needAsyncCheck = defaultTarget <= _bcIndex ? true : false;
   TR::Node * caseNode = TR::Node::createCase( 0, genTarget(defaultTarget));
   TR::Node * node = TR::Node::create(TR::lookup, tableSize + 2, first, caseNode);

   for (i = 0; i < tableSize; ++i)
      {
      int32_t intMatch = nextSwitchValue(bcIndex);
      int32_t target = nextSwitchValue(bcIndex) + _bcIndex;
      if (target <= _bcIndex)
         needAsyncCheck = true;
      node->setAndIncChild(i + 2, TR::Node::createCase( 0, genTarget(target), intMatch));
      }

   if (needAsyncCheck)
      genAsyncCheck();
   genTreeTop(node);
   return findNextByteCodeToGen();
   }

int32_t
TR_J9ByteCodeIlGenerator::genTableSwitch()
   {
   int32_t i = 1;
   while ((intptrj_t)&_code[_bcIndex+i] & 3) ++i; // 4 byte align

   int32_t bcIndex = _bcIndex + i;
   int32_t defaultTarget = nextSwitchValue(bcIndex) + _bcIndex;
   int32_t low = nextSwitchValue(bcIndex);
   int32_t high = nextSwitchValue(bcIndex);
   if (low != 0)
      {
      loadConstant(TR::iconst, low);
      genBinary(TR::isub);
      high = high - low;
      }

   TR::Node * first = pop();

   handlePendingPushSaveSideEffects(first);

   bool needAsyncCheck = defaultTarget <= _bcIndex ? true : false;
   TR::Node * caseNode = TR::Node::createCase(0, genTarget(defaultTarget));
   TR::Node * node = TR::Node::create(TR::table, high + 3, first, caseNode);

   TR_Array<TR::Node *> caseTargets(trMemory(), _maxByteCodeIndex + 1, true, stackAlloc);
   for (i = 0; i < high + 1; ++i)
      {
      int32_t targetIndex = nextSwitchValue(bcIndex) + _bcIndex;
      if (targetIndex <= _bcIndex)
         needAsyncCheck = true;
      if (!caseTargets[targetIndex])
         caseTargets[targetIndex] = TR::Node::createCase(0, genTarget(targetIndex));
      node->setAndIncChild(i + 2, caseTargets[targetIndex]);
      }

   if (needAsyncCheck)
      genAsyncCheck();
   genTreeTop(node);
   return findNextByteCodeToGen();
   }

//----------------------------------------------
// gen throw
//----------------------------------------------

int32_t
TR_J9ByteCodeIlGenerator::genAThrow()
   {
   TR::Node * node = TR::Node::createWithSymRef(TR::athrow, 1, 1, pop(), symRefTab()->findOrCreateAThrowSymbolRef(_methodSymbol));

   bool canSkipNullCheck = node->getFirstChild()->isNonNull();
   if (!canSkipNullCheck && _classInfo)
      {
      if (!_classInfo->getFieldInfo())
         performClassLookahead(_classInfo);
      TR::Node * thisObject = node->getFirstChild();
      TR_PersistentFieldInfo * fieldInfo = _classInfo->getFieldInfo() ? _classInfo->getFieldInfo()->findFieldInfo(comp(), thisObject, false) : NULL;
      if (fieldInfo && fieldInfo->isTypeInfoValid())
         {
         if (performTransformation(comp(), "O^O CLASS LOOKAHEAD: Can skip null check at exception throw %p based on class file examination\n", thisObject))
            canSkipNullCheck = true;
         }
      }

    if (comp()->getOption(TR_EnableThisLiveRangeExtension))
      {
      if (!_methodSymbol->isStatic() &&
          (!fej9()->isClassFinal(_methodSymbol->getResolvedMethod()->containingClass()) ||
           fej9()->hasFinalizer(_methodSymbol->getResolvedMethod()->containingClass())))
         {
         loadAuto(TR::Address, 0);
         TR::SymbolReference *tempSymRef = symRefTab()->findOrCreateThisRangeExtensionSymRef(comp()->getMethodSymbol());
         genTreeTop(TR::Node::createStore(tempSymRef, pop()));
         }
      }

   if (!canSkipNullCheck)
      node = genNullCheck(node);

   genTreeTop(node);

   discardEntireStack();

   return findNextByteCodeToGen();
   }

// genFlush: provide a store barrier for weakly consistent memory system//
//
void TR_J9ByteCodeIlGenerator::genFlush(int32_t nargs)
   {
   if (cg()->getEnforceStoreOrder())
      {
      TR::Node *newNode = node(_stack->topIndex() - nargs);
      TR::Node *flushNode = TR::Node::createAllocationFence(NULL, newNode);

      genTreeTop(flushNode);
      }
   }

void TR_J9ByteCodeIlGenerator::genFullFence(TR::Node *node)
   {
   TR::Node *fullFenceNode = TR::Node::createWithSymRef(node, TR::fullFence, 0, node->getSymbolReference());
   fullFenceNode->setOmitSync(true);
   genTreeTop(fullFenceNode);
   }


void TR_J9ByteCodeIlGenerator::performClassLookahead(TR_PersistentClassInfo *classInfo)
   {
   // Do not perform class lookahead when peeking (including recursive class lookahead)
   //
   if (comp()->isPeekingMethod())
      return;
   // Do not perform class lookahead if classes can be redefined
   if (comp()->getOption(TR_EnableHCR))
      return;

   if (comp()->compileRelocatableCode())
      return;

   _classLookaheadSymRefTab = new (trStackMemory())TR::SymbolReferenceTable(method()->maxBytecodeIndex(), comp());

   TR::SymbolReferenceTable *callerCurrentSymRefTab = comp()->getCurrentSymRefTab();
   comp()->setCurrentSymRefTab(_classLookaheadSymRefTab);
   TR_ClassLookahead classLookahead(classInfo, fej9(), comp(), _classLookaheadSymRefTab);
   classLookahead.perform();
   comp()->setCurrentSymRefTab(callerCurrentSymRefTab);
   }
