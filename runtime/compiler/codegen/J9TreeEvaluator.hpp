/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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
#include "codegen/Snippet.hpp"

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

   static TR::Register *zdloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2zdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zd2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsleLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdslsLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdstsLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsleLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdslsLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdstsLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsleStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdslsStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdstsStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsleStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdslsStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdstsStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zd2zdsleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zd2zdslsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zd2zdstsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsle2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsls2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsts2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsle2zdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsls2zdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zdsts2zdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2zdslsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2zdslsSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2zdstsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2zdstsSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udslLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udstLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udslLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udstLoadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udslStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udstStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udslStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udstStoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2udEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2udslEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2udstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udsl2udEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udst2udEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ud2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udsl2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *udst2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pddivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdabsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdshrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdshrSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdshlSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdshlOverflowEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdchkEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2iuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2iOverflowEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iu2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2luEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2lOverflowEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lu2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pd2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2pdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdcleanEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdclearEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdclearSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdSetSignEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *pdModifyPrecisionEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *countDigitsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BCDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static uint32_t calculateInstanceOfOrCheckCastSequences(TR::Node *instanceOfOrCheckCastNode, InstanceOfOrCheckCastSequences *sequences, TR_OpaqueClassBlock **compileTimeGuessClass, TR::CodeGenerator *cg, InstanceOfOrCheckCastProfiledClasses *profiledClassList, uint32_t *numberOfProfiledClass, uint32_t maxProfileClass, float *topClassProbability, bool *topClassWasCastClass);

   static uint8_t interpreterProfilingInstanceOfOrCheckCastInfo(TR::CodeGenerator * cg, TR::Node * node, TR_OpaqueClassBlock **classArray, float* probability=NULL, bool recordAll = false);

   static TR_OpaqueClassBlock* interpreterProfilingInstanceOfOrCheckCastInfo(TR::CodeGenerator * cg, TR::Node * node);

   static bool checkcastShouldOutlineSuperClassTest(TR::Node *castClassNode, TR::CodeGenerator *cg);

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
   static TR::Register *monexitfenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   /*
    * \brief
    *   Invokes the sequence of codegen routines to report the watched field.
    */
   static void rdWrtbarHelperForFieldWatch(TR::Node *node, TR::CodeGenerator *cg, TR::Register *sideEffectRegister, TR::Register *valueReg);
   /*
    * \brief
    *   Populates and Returns the FieldWatchStaticSnippet corresponding to the specific codegen.
    */
   static TR::Snippet *getFieldWatchStaticSnippet(TR::CodeGenerator *cg, TR::Node *node, J9Method *m, UDATA loc, void *fieldAddress, J9Class *fieldClass);
   /*
    * \brief
    *   Populates and Returns the FieldWatchInstanceSnippet corresponding to the specific codegen.
    */
   static TR::Snippet *getFieldWatchInstanceSnippet(TR::CodeGenerator *cg, TR::Node *node, J9Method *m, UDATA loc, UDATA os);
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

   static TR::Register *resolveCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   /*
    * \brief
    *  Checks if inline allocation is allowed when value type is enabled.
    *  The helper call is required if the first child of the "new" node, the class node, is a value type class
    *  but it tries to create a non-value type instance, or if the class node is non-value type class but it
    *  tries to create a value type instance.
    */
   static bool requireHelperCallValueTypeAllocation(TR::Node *node, TR::CodeGenerator *cg);
   };

}

#endif
