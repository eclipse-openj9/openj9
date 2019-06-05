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

#ifndef J9_TREE_EVALUATOR_INCL
#define J9_TREE_EVALUATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_TREE_EVALUATOR_CONNECTOR
#define J9_TREE_EVALUATOR_CONNECTOR
namespace J9 { class TreeEvaluator; }
namespace J9 { typedef J9::TreeEvaluator TreeEvaluatorConnector; }
#endif

#include "codegen/OMRTreeEvaluator.hpp"

namespace J9
{

class OMR_EXTENSIBLE TreeEvaluator: public OMR::TreeEvaluatorConnector
   {
   public:
   enum InstanceOfOrCheckCastSequences
      {
      EvaluateCastClass,
      LoadObjectClass,

      NullTest,                         // Needs object class: n, needs cast class: n
      GoToTrue,                         // Needs object class: n, needs cast class: n
      GoToFalse,                        // Needs object class: n, needs cast class: n
      ProfiledClassTest,                // Needs object class: y, needs cast class: n
      CompileTimeGuessClassTest,        // Needs object class: y, needs cast class: n
      ArrayOfJavaLangObjectTest,        // Needs object class: y, needs cast class: n
      ClassEqualityTest,                // Needs object class: y, needs cast class: y
      SuperClassTest,                   // Needs object class: y, needs cast class: y
      CastClassCacheTest,               // Needs object class: y, needs cast class: y
      DynamicCacheObjectClassTest,      // Needs object class: y, needs cast class: n
      DynamicCacheDynamicCastClassTest, // Needs object class: y, needs cast class: y
      HelperCall,                       // Needs object class: n, needs cast class: y

      InstanceOfOrCheckCastMaxSequences
      };
   
   /** \brief
     *   A structure containing following information
     *   1. class
     *   2. boolean representing if the class is instanceof cast class or not
     *   3. probability
     */    
   struct InstanceOfOrCheckCastProfiledClasses
      {
      TR_OpaqueClassBlock *profiledClass;
      bool isProfiledClassInstanceOfCastClass;
      float frequency;
      };
 
 
   static uint32_t calculateInstanceOfOrCheckCastSequences(TR::Node *instanceOfOrCheckCastNode, InstanceOfOrCheckCastSequences *sequences, TR_OpaqueClassBlock **compileTimeGuessClass, TR::CodeGenerator *cg, InstanceOfOrCheckCastProfiledClasses *profiledClassList, uint32_t *numberOfProfiledClass, uint32_t maxProfileClass, float *topClassProbability, bool *topClassWasCastClass);

   static uint8_t interpreterProfilingInstanceOfOrCheckCastInfo(TR::CodeGenerator * cg, TR::Node * node, TR_OpaqueClassBlock **classArray, float* probability=NULL, bool recordAll = false);

   static TR_OpaqueClassBlock* interpreterProfilingInstanceOfOrCheckCastInfo(TR::CodeGenerator * cg, TR::Node * node);

   static bool checkcastShouldOutlineSuperClassTest(TR::Node *castClassNode, TR::CodeGenerator *cg);

   static bool loadLookaheadAfterHeaderAccess(TR::Node *node, int32_t &fieldOffset, TR::CodeGenerator *cg);

   static bool instanceOfNeedHelperCall(bool testCastClassIsSuper, bool isFinalClass);

   static bool instanceOfOrCheckCastIsJavaLangObjectArray(TR::Node * castClassNode, TR::CodeGenerator *cg);
   static TR_OpaqueClassBlock * getCastClassAddress(TR::Node * castClassNode);
   static bool instanceOfOrCheckCastIsFinalArray(TR::Node * castClassNode, TR::CodeGenerator *cg);

   static TR::Register *bwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *swrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *swrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *frdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *frdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *drdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *drdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *brdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *brdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *srdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *srdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lrdbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lrdbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static void rdWrtbarHelperForFieldWatch(TR::Node *node, TR::CodeGenerator *cg, TR::Register *sideEffectRegister, TR::Register *valueReg); 

   /*
    * \brief
    *   Sets the node representing the value to be written for an indirect wrtbar. Returns
    *   true/false if the correct pattern was found (i.e. if compressedReferences is being used).
    *
    * \note
    *   For address type nodes using compressedrefs, the compressed refs sequence is skipped
    *   from the sub tree and the uncompressed address value is set to sourceChild.
    *   For all other cases, sourceChild is set to the secondChild.
    */
   static bool getIndirectWrtbarValueNode(TR::CodeGenerator *cg, TR::Node *node, TR::Node*& sourceChild, bool incSrcRefCount);
   static void evaluateLockForReservation(TR::Node *node, bool *reservingLock, bool *normalLockPreservingReservation, TR::CodeGenerator *cg);
   static bool isPrimitiveMonitor (TR::Node *node, TR::CodeGenerator *cg);
   static bool isDummyMonitorEnter (TR::Node *node, TR::CodeGenerator *cg);
   static bool isDummyMonitorExit (TR::Node *node, TR::CodeGenerator *cg);

   static void preEvaluateEscapingNodesForSpineCheck(TR::Node *root, TR::CodeGenerator *cg);

   };

}

#endif
