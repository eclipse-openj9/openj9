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

#ifndef DATAACCESSACCELERATOR_INCL
#define DATAACCESSACCELERATOR_INCL

#include <stddef.h>
#include <stdint.h>
#include <vector>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/ILWalk.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"

namespace TR { class TreeTop; }

/** \brief
 *
 *  Transforms calls to recognized Data Access Accelerator (DAA) library methods into hardware semantically
 *  equivalent hardware intrinsics for the underlying platform.
 *
 *  \details
 *
 *  The Data Access Accelerator (DAA) library (com/ibm/dataaccess) found in IBM J9 Virtual Machine is a utility
 *  library for performing hardware accelerated operations on Java data types. All library methods have a Java
 *  implementation, however if hardware support exists for a particular operation the JIT compiler will attempt
 *  to replace calls to such library methods with semantically equivalent hardware intrinsics.
 *
 *  The DAA library method calls that are actually replaced with hardware intrinsics are not the publically visible
 *  API methods. Instead we replace the private "underscore" counterparts to the public API so as to guard us for
 *  possible future modifications of such private methods. For example the
 *  com/ibm/dataaccess/ByteArrayMarshaller.writeInt(I[BIZ)V method calls a private
 *  com/ibm/dataaccess/ByteArrayMarshaller.writeInt_(I[BIZ)V method which carries out the write operation. The
 *  latter method is the so called "underscore" method which we recognize and hardware accelerate.

 *  Note that if hardware acceleration support is detected we prevent the inlining of such "underscore" methods
 *  in the inliner on the assumption that this optimization will reduce such calls to simple semantically
 *  equivalent trees which will outperform the inlined call.
 *
 *  The DAA library is broken up into four main classes and hardware support is defined per method as follows:
 *
 *  \section com.ibm.data.ByteArrayMarshaller
 *
 *  The following methods have hardware acceleration support on x86 (Linux and Windows), PPC (Linux and AIX),
 *  and System z (Linux and z/OS):
 *
 *  - com/ibm/dataaccess/ByteArrayMarshaller.writeShort_(S[BIZ)V
 *  - com/ibm/dataaccess/ByteArrayMarshaller.writeShort_(S[BIZI)V
 *  - com/ibm/dataaccess/ByteArrayMarshaller.writeInt_(I[BIZ)V
 *  - com/ibm/dataaccess/ByteArrayMarshaller.writeInt_(I[BIZI)V
 *  - com/ibm/dataaccess/ByteArrayMarshaller.writeLong_(J[BIZ)V
 *  - com/ibm/dataaccess/ByteArrayMarshaller.writeLong_(J[BIZI)V
 *  - com/ibm/dataaccess/ByteArrayMarshaller.writeFloat_(F[BIZ)V
 *  - com/ibm/dataaccess/ByteArrayMarshaller.writeDouble_(D[BIZ)V
 *
 *  \section com.ibm.data.ByteArrayUnMarshaller
 *
 *  The following methods have hardware acceleration support on x86 (Linux and Windows), PPC (Linux and AIX),
 *  and System z (Linux and z/OS):
 *
 *  - com/ibm/dataaccess/ByteArrayUnmarshaller.readShort_([BIZ)S
 *  - com/ibm/dataaccess/ByteArrayUnmarshaller.readShort_([BIZIZ)S
 *  - com/ibm/dataaccess/ByteArrayUnmarshaller.readInt_([BIZ)I
 *  - com/ibm/dataaccess/ByteArrayUnmarshaller.readInt_([BIZIZ)I
 *  - com/ibm/dataaccess/ByteArrayUnmarshaller.readLong_([BIZ)J
 *  - com/ibm/dataaccess/ByteArrayUnmarshaller.readLong_([BIZIZ)J
 *  - com/ibm/dataaccess/ByteArrayUnmarshaller.readFloat_([BIZ)F
 *  - com/ibm/dataaccess/ByteArrayUnmarshaller.readDouble_([BIZ)D
 *
 *  \section com.ibm.data.DecimalData
 *
 *  The following methods have hardware acceleration support on System z (Linux and z/OS):
 *
 *  - com/ibm/dataaccess/DecimalData.convertPackedDecimalToUnicodeDecimal_([BI[CIII)V
 *  - com/ibm/dataaccess/DecimalData.convertUnicodeDecimalToPackedDecimal_([CI[BIII)V
 *  - com/ibm/dataaccess/DecimalData.convertPackedDecimalToExternalDecimal_([BI[BIII)V
 *  - com/ibm/dataaccess/DecimalData.convertExternalDecimalToPackedDecimal_([BI[BIII)V
 *
 *  The following methods have hardware acceleration support on System z (z/OS):
 *
 *  - com/ibm/dataaccess/DecimalData.convertPackedDecimalToInteger_([BIIZ)I
 *  - com/ibm/dataaccess/DecimalData.convertPackedDecimalToInteger_(Ljava/nio/ByteBuffer;IIZJII)I
 *  - com/ibm/dataaccess/DecimalData.convertIntegerToPackedDecimal_(I[BIIZ)V
 *  - com/ibm/dataaccess/DecimalData.convertIntegerToPackedDecimal_(ILjava/nio/ByteBuffer;IIZJII)V
 *  - com/ibm/dataaccess/DecimalData.convertPackedDecimalToLong_([BIIZ)J
 *  - com/ibm/dataaccess/DecimalData.convertPackedDecimalToLong_(Ljava/nio/ByteBuffer;IIZJII)J
 *  - com/ibm/dataaccess/DecimalData.convertLongToPackedDecimal_(J[BIIZ)V
 *  - com/ibm/dataaccess/DecimalData.convertLongToPackedDecimal_(JLjava/nio/ByteBuffer;IIZJII)V
 *
 *  \section com.ibm.data.PackedDecimal
 *
 *  The following methods have hardware acceleration support on System z (Linux and z/OS):
 *
 *  - com/ibm/dataaccess/PackedDecimal.checkPackedDecimal_([BIIZZ)I
 *
 *  The following methods have hardware acceleration support on System z (z/OS):
 *
 *  - com/ibm/dataaccess/PackedDecimal.addPackedDecimal_([BII[BII[BIIZ)V
 *  - com/ibm/dataaccess/PackedDecimal.subtractPackedDecimal_([BII[BII[BIIZ)V
 *  - com/ibm/dataaccess/PackedDecimal.multiplyPackedDecimal_([BII[BII[BIIZ)V
 *  - com/ibm/dataaccess/PackedDecimal.dividePackedDecimal_([BII[BII[BIIZ)V
 *  - com/ibm/dataaccess/PackedDecimal.remainderPackedDecimal_([BII[BII[BIIZ)V
 *  - com/ibm/dataaccess/PackedDecimal.shiftLeftPackedDecimal_([BII[BIIIZ)V
 *  - com/ibm/dataaccess/PackedDecimal.shiftRightPackedDecimal_([BII[BIIIZ)V
 *  - com/ibm/dataaccess/PackedDecimal.lessThanPackedDecimal_([BII[BII)Z
 *  - com/ibm/dataaccess/PackedDecimal.lessThanOrEqualsPackedDecimal_([BII[BII)Z
 *  - com/ibm/dataaccess/PackedDecimal.greaterThanPackedDecimal_([BII[BII)Z
 *  - com/ibm/dataaccess/PackedDecimal.greaterThanOrEqualsPackedDecimal_([BII[BII)Z
 *  - com/ibm/dataaccess/PackedDecimal.equalsPackedDecimal_([BII[BII)Z
 *
 */
class TR_DataAccessAccelerator : public TR::Optimization
   {
   public:
   typedef TR::typed_allocator< TR::TreeTop *, TR::Region & > TreeTopContainerAllocator;
   typedef std::vector< TR::TreeTop*, TreeTopContainerAllocator > TreeTopContainer;
   typedef TR::typed_allocator< TR::Block *, TR::Region & > BlockContainerAllocator;
   typedef std::vector< TR::Block*, BlockContainerAllocator > BlockContainer;

   TR_DataAccessAccelerator(TR::OptimizationManager* manager);

   /** \brief
    *     Helper function to create an instance of the StringBuilderTransformer optimization using the
    *     OptimizationManager's default allocator.
    *
    *  \param manager
    *     The optimization manager.
    */
   static TR::Optimization* create(TR::OptimizationManager* manager)
      {
      return new (manager->allocator()) TR_DataAccessAccelerator(manager);
      }

   /** \brief
    *     Performs the optimization on this compilation unit.
    *
    *  \return
    *     1 if any transformation was performed; 0 otherwise.
    */
   virtual int32_t perform();

   /** \brief
    *     Performs the optimization on a specific block within this compilation unit.
    *
    *  \param block
    *     The block on which to perform this optimization.
    *
    *  \param variableCallTreeTops
    *     A vector of TR::TreeTop*. Used to build a list of variable precision calls to be used
    *     later
    *
    *  \return
    *     1 if any transformation was performed; 0 otherwise.
    */
   virtual int32_t performOnBlock(TR::Block* block, TreeTopContainer* variableCallTreeTops);

   /** \brief
    *     Performs inlining of variable precision API calls after iterating through the entire tree
    *
    *  \detail
    *     Unlike constant precision DAA call inlining which can be done in-place without introducing extra blocks,
    *     each variable precision call node has to be bloated into multiple
    *     basic blocks to form a precision diamond. This disrupts the CFG and invalidates block iterator and
    *     TreeTop iterator that might be in use. As a result of this, it's difficult to inline variable precision
    *     calls while iterating the entire tree. The solution to this problem is to build a list of variable
    *     precision TreeTops during the tree traversal phase. And after that, go through this list and inline each one of them.
    *
    *  \param variableCallTreeTops
    *     A vector of TR::TreeTop*. All variable precision calls listed here will be inlined.
    *
    *  \return
    *     1 if any transformation was performed; 0 otherwise.
    */
   virtual int32_t processVariableCalls(TreeTopContainer* variableCallTreeTops);

   virtual const char * optDetailString() const throw();

   bool isChildConst (TR::Node* node, int32_t child);

   TR::Node* insertIntegerGetIntrinsic(TR::TreeTop* callTreeTop, TR::Node* callNode, int32_t sourceNumBytes, int32_t targetNumBytes);
   TR::Node* insertIntegerSetIntrinsic(TR::TreeTop* callTreeTop, TR::Node* callNode, int32_t sourceNumBytes, int32_t targetNumBytes);

   TR::Node* insertDecimalGetIntrinsic(TR::TreeTop* callTreeTop, TR::Node* callNode, int32_t sourceNumBytes, int32_t targetNumBytes);
   TR::Node* insertDecimalSetIntrinsic(TR::TreeTop* callTreeTop, TR::Node* callNode, int32_t sourceNumBytes, int32_t targetNumBytes);

   bool inlineCheckPackedDecimal(TR::TreeTop* callTreeTop, TR::Node* callNode);

   private:

   TR::Node* constructAddressNode(TR::Node* callNode, TR::Node* arrayNode, TR::Node* offsetNode);

   void createPrecisionDiamond(TR::Compilation* comp,
                               TR::TreeTop* treeTop,
                               TR::TreeTop* fastTree, TR::TreeTop* slowTree,
                               bool isPD2I,
                               uint32_t numPrecisionNodes,
                               ...);

   TR::Node* restructureVariablePrecisionCallNode(TR::TreeTop* treeTop, TR::Node* callNode);

   bool generatePD2I(TR::TreeTop* treeTop, TR::Node* callNode, bool isPD2i, bool isByteBuffer);
   bool generatePD2IVariableParameter(TR::TreeTop* treeTop, TR::Node* callNode, bool isPD2i, bool isByteBuffer);
   bool generatePD2IConstantParameter(TR::TreeTop* treeTop, TR::Node* callNode, bool isPD2i, bool isByteBuffer);
   bool generateI2PD(TR::TreeTop* treeTop, TR::Node* callNode, bool isI2PD, bool isByteBuffer);
   bool genArithmeticIntrinsic(TR::TreeTop* treeTop, TR::Node* callNode, TR::ILOpCodes opCode);
   bool genComparisionIntrinsic(TR::TreeTop* treeTop, TR::Node* callNode, TR::ILOpCodes opCode);
   bool genShiftLeftIntrinsic(TR::TreeTop* treeTop, TR::Node* callNode);
   bool genShiftRightIntrinsic(TR::TreeTop* treeTop, TR::Node* callNode);
   bool generateUD2PD(TR::TreeTop* treeTop, TR::Node* callNode, bool isUD2PD);
   bool generatePD2UD(TR::TreeTop* treeTop, TR::Node* callNode, bool isPD2UD);

   void insertByteArrayNULLCHK(TR::TreeTop* callTreeTop, TR::Node* callNode, TR::Node* byteArrayNode);
   void insertByteArrayBNDCHK(TR::TreeTop* callTreeTop, TR::Node* callNode, TR::Node* byteArrayNode, TR::Node* offsetNode, int32_t index);

   TR::Node* createByteArrayElementAddress(TR::TreeTop* callTreeTop, TR::Node* callNode, TR::Node* byteArrayNode, TR::Node* offsetNode);

   bool printInliningStatus(bool status, TR::Node* node, const char* reason = "")
      {
      if (status)
         traceMsg(comp(), "DataAccessAccelerator: Intrinsics on node %p : SUCCESS\n", node);
      else
         {
         traceMsg(comp(), "DataAccessAccelerator: Intrinsics on node %p : FAILED\n", node);
         traceMsg(comp(), "DataAccessAccelerator:     Reason : %s\n", reason);
         }

      return status;
      }
   };

#endif
