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

#include "optimizer/J9ValuePropagation.hpp"
#include "optimizer/VPBCDConstraint.hpp"
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "env/CHTable.hpp"
#include "env/ClassTableCriticalSection.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/VMJ9.h"
#include "optimizer/Optimization_inlines.hpp"
#include "env/j9method.h"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "infra/Cfg.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/CompilerEnv.hpp"
#include "optimizer/TransformUtil.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "env/J9JitMemory.hpp"
#include "optimizer/HCRGuardAnalysis.hpp"

#define OPT_DETAILS "O^O VALUE PROPAGATION: "

J9::ValuePropagation::ValuePropagation(TR::OptimizationManager *manager)
   : OMR::ValuePropagation(manager),
     _bcdSignConstraints(NULL),
     _callsToBeFoldedToIconst(trMemory())
   {
   }

// _bcdSignConstraints is an array organized first by TR::DataType and then by TR_BCDSignConstraint
// getBCDSignConstraints returns a pointer to the first entry for a given dt.
// e.g.:
// 0 : TR::PackedDecimal - TR_Sign_Unknown       <- return when dt=TR::PackedDecimal
// 1 : TR::PackedDecimal - TR_Sign_Clean
// ...
// 6 : TR::PackedDecimal - TR_Sign_Minus_Clean
// 7 : TR::ZonedDecimal  - TR_Sign_Unknown       <- return when dt=TR::ZonedDecimal
// ...
// 13: TR::ZonedDecimal  - TR_Sign_Minus_Clean
// ...
// ...
// xx: TR::LastBCDType   - TR_Sign_Unknown       <- return when dt=TR::LastBCDType
//  ...
// zz: TR::LastBCDType   - TR_Sign_Minus_Clean
//
TR::VP_BCDSign **J9::ValuePropagation::getBCDSignConstraints(TR::DataType dt)
   {
   TR_ASSERT(dt.isBCD(),"dt %s not a bcd type\n",dt.toString());
   if (_bcdSignConstraints == NULL)
      {
      size_t size = TR::DataType::numBCDTypes() * TR_Sign_Num_Types * sizeof(TR::VP_BCDSign*);
      _bcdSignConstraints = (TR::VP_BCDSign**)trMemory()->allocateStackMemory(size);
      memset(_bcdSignConstraints, 0, size);
//    traceMsg(comp(),"create _bcdSignConstraints %p of size %d (numBCDTypes %d * TR_Sign_Num_Types %d * sizeof(TR::VP_BCDSign*) %d)\n",
//       _bcdSignConstraints,size,TR::DataType::numBCDTypes(),TR_Sign_Num_Types,sizeof(TR::VP_BCDSign*));
      }
//   traceMsg(comp(),"lookup dt %d, ret %p + %d = %p\n",
//      dt,_bcdSignConstraints,(dt-TR::FirstBCDType)*TR_Sign_Num_Types,_bcdSignConstraints + (dt-TR::FirstBCDType)*TR_Sign_Num_Types);
   return _bcdSignConstraints + (dt-TR::FirstBCDType)*TR_Sign_Num_Types;
   }

/**
 * \brief
 *    Generate a diamond for a call with the guard being an HCR guard, the fast path being an iconst node
 *    and the slow path being the original call.
 *
 * \parm callTree
 *    Tree of call to be folded.
 *
 * \parm result
 *    The constant used to replace the call in the fast path.
 */
void
J9::ValuePropagation::transformCallToIconstWithHCRGuard(TR::TreeTop *callTree, int32_t result)
   {
   static const char *disableHCRGuards = feGetEnv("TR_DisableHCRGuards");
   TR_ASSERT(!disableHCRGuards && comp()->getHCRMode() != TR::none, "foldCallToConstantInHCRMode should be called in HCR mode");

   TR::Node * callNode = callTree->getNode()->getFirstChild();
   TR_ASSERT(callNode->getSymbol()->isResolvedMethod(), "Expecting resolved call in transformCallToIconstWithHCRGuard");

   TR::ResolvedMethodSymbol *calleeSymbol = callNode->getSymbol()->castToResolvedMethodSymbol();

   // Add the call to inlining table
   if (!comp()->incInlineDepth(calleeSymbol, callNode->getByteCodeInfo(), callNode->getSymbolReference()->getCPIndex(), callNode->getSymbolReference(), !callNode->getOpCode().isCallIndirect(), 0))
      {
      if (trace())
         traceMsg(comp(), "Cannot inline call %p, quit transforming it into a constant\n", callNode);
      return;
      }
   // With the method added to the inlining table, getCurrentInlinedSiteIndex() will return the desired inlined site index
   int16_t calleeIndex = comp()->getCurrentInlinedSiteIndex();
   TR::Node *compareNode= TR_VirtualGuard::createHCRGuard(comp(), calleeIndex, callNode, NULL, calleeSymbol, calleeSymbol->getResolvedMethod()->classOfMethod());

   // ifTree is a duplicate of the callTree, create temps for the children to avoid commoning
   J9::TransformUtil::createTempsForCall(this, callTree);
   TR::TreeTop *compareTree = TR::TreeTop::create(comp(), compareNode);
   TR::TreeTop *ifTree = TR::TreeTop::create(comp(),callTree->getNode()->duplicateTree());
   ifTree->getNode()->getFirstChild()->setIsTheVirtualCallNodeForAGuardedInlinedCall();
   // resultNode is the inlined node, should have the correct callee index
   // Pass compareNode as the originatingByteCodeNode so that the resultNode has the correct callee index
   TR::Node *resultNode = TR::Node::iconst(compareNode, result);
   TR::TreeTop *elseTree = TR::TreeTop::create(comp(), TR::Node::create(callNode, TR::treetop, 1, resultNode));
   J9::TransformUtil::createDiamondForCall(this, callTree, compareTree, ifTree, elseTree, false /*changeBlockExtensions*/, true /*markCold*/);
   comp()->decInlineDepth();
   }

/**
 * \brief
 *    Return a pointer to the object reference if applicable.
 *
 * \parm constraint
 *    The VP constraint from which an object location is asked for.
 *
 * \return
 *    Return a pointer to the object reference if the constraint is a constString or a known object constraint,
 *    otherwise return NULL.
 */
uintptr_t*
J9::ValuePropagation::getObjectLocationFromConstraint(TR::VPConstraint *constraint)
   {
    uintptr_t* objectLocation = NULL;
    if (constraint->isConstString())
       {
       // VPConstString constraint, the symref is resolved for VPConstString constraint
       objectLocation = (uintptr_t*)constraint->getClassType()->asConstString()->getSymRef()->getSymbol()->castToStaticSymbol()->getStaticAddress();
       }
    else if (constraint->getKnownObject())
       {
       TR::KnownObjectTable::Index index = constraint->getKnownObject()->getIndex();
       if (index != TR::KnownObjectTable::UNKNOWN)
          objectLocation = comp()->getKnownObjectTable()->getPointerLocation(index);
       }
   return objectLocation;
   }

/**
 * \brief
 *    Fold a call to the given result in place or add it to the worklist of delayed transformations.
 *
 * \parm callTree
 *    The tree containing the call to be folded.
 *
 * \parm result
 *    The constant used to replace the call.
 *
 * \parm isGlobal
 *    If true, add global constraint to the new node if call is folded in place.
 *
 * \parm inPlace
 *   If true, fold the call in place. Otherwise in delayed transformations.
 */
void
J9::ValuePropagation::transformCallToIconstInPlaceOrInDelayedTransformations(TR::TreeTop* callTree, int32_t result, bool isGlobal, bool inPlace)
   {
    TR::Node * callNode = callTree->getNode()->getFirstChild();
    TR::Method * calledMethod = callNode->getSymbol()->castToMethodSymbol()->getMethod();
    const char *signature = calledMethod->signature(comp()->trMemory(), stackAlloc);
    if (inPlace)
       {
       if (trace())
          traceMsg(comp(), "Fold the call to %s on node %p to %d\n", signature, callNode, result);
       replaceByConstant(callNode, TR::VPIntConst::create(this, result), isGlobal);
       }
    else
       {
       if (trace())
          traceMsg(comp(), "The call to %s on node %p will be folded to %d in delayed transformations\n", signature, callNode, result);
       _callsToBeFoldedToIconst.add(new (trStackMemory()) TreeIntResultPair(callTree, result));
       }
  }

/**
 * \brief
 *    Check if the given constraint is for a java/lang/String object.
 *
 * \parm constraint
 *    The VP constraint to be checked.
 *
 * \return
 *    Return TR_yes if the constraint is for a java/lang/String object,
 *    TR_no if it is not, TR_maybe if it is unclear.
 */
TR_YesNoMaybe
J9::ValuePropagation::isStringObject(TR::VPConstraint *constraint)
   {
   if (constraint
       && constraint->getClassType()
       && constraint->getClass()
       && constraint->getClassType()->asResolvedClass())
      {
      if (constraint->isClassObject() == TR_yes)
         return TR_no;
      if (constraint->isClassObject() == TR_no)
         {
         TR_OpaqueClassBlock* objectClass = constraint->getClass();
         if (constraint->getClassType()->asFixedClass())
            return comp()->fej9()->isString(objectClass) ? TR_yes : TR_no;
         else
            return comp()->fej9()->typeReferenceStringObject(objectClass);
         }
      }
   return TR_maybe;
   }

/**
 * \brief
 *    Check if the given constraint indicates a known java/lang/String object.
 *
 * \parm constraint
 *    The VP constraint to be checked.
 *
 * \return
 *    Return true if the constraint indicates a known java/lang/String object, false otherwise.
 */
bool
J9::ValuePropagation::isKnownStringObject(TR::VPConstraint *constraint)
   {
   return isStringObject(constraint) == TR_yes
          && constraint->isNonNullObject()
          && (constraint->isConstString() || constraint->getKnownObject());
   }

void
J9::ValuePropagation::constrainRecognizedMethod(TR::Node *node)
   {
   // Only constrain resolved calls
   if (!node->getSymbol()->isResolvedMethod())
      return;

   const static char *disableVPFoldRecognizedMethod = feGetEnv("TR_disableVPFoldRecognizedMethod");
   if (disableVPFoldRecognizedMethod)
      return;
   TR::ResolvedMethodSymbol* symbol = node->getSymbol()->getResolvedMethodSymbol();
   TR::RecognizedMethod rm = symbol->getRecognizedMethod();
   if(rm == TR::unknownMethod)
      return;
   // Do not constraint a call node for a guarded inlined call
   if (node->isTheVirtualCallNodeForAGuardedInlinedCall())
      return;

   TR_ResolvedMethod* calledMethod = symbol->getResolvedMethod();
   const char *signature = calledMethod->signature(comp()->trMemory(), stackAlloc);

   static const char *disableHCRGuards = feGetEnv("TR_DisableHCRGuards");
   bool transformNonnativeMethodInPlace = disableHCRGuards || comp()->getHCRMode() == TR::none;

   if (trace())
      traceMsg(comp(), "Trying to compute the result of call to %s on node %p at compile time\n", signature, node);
   // This switch is used for transformations with AOT support
   switch (rm)
      {
      case TR::java_lang_Class_isInterface:
         {
         // Only constrain the call in the last run of vp to avoid adding the candidate twice if the call is inside a loop
         if (!lastTimeThrough())
            return;
         TR::Node *receiverChild = node->getLastChild();
         bool receiverChildGlobal;
         TR::VPConstraint *receiverChildConstraint = getConstraint(receiverChild, receiverChildGlobal);
         if (receiverChildConstraint
             && receiverChildConstraint->isJavaLangClassObject() == TR_yes
             && receiverChildConstraint->isNonNullObject()
             && receiverChildConstraint->getClassType()
             && receiverChildConstraint->getClassType()->asFixedClass())
            {
            int32_t isInterface = TR::Compiler->cls.isInterfaceClass(comp(), receiverChildConstraint->getClass());
            transformCallToIconstInPlaceOrInDelayedTransformations(_curTree, isInterface, receiverChildGlobal, transformNonnativeMethodInPlace);
            TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
            return;
            }
         break;
         }
      case TR::java_lang_String_hashCode:
         {
         // Only constrain the call in the last run of vp to avoid adding the candidate twice if the call is inside a loop
         if (!lastTimeThrough())
            return;
         TR::Node *receiverChild = node->getLastChild();
         bool receiverChildGlobal;
         TR::VPConstraint *receiverChildConstraint= getConstraint(receiverChild, receiverChildGlobal);
         if (isKnownStringObject(receiverChildConstraint))
            {
            uintptr_t* stringLocation = getObjectLocationFromConstraint(receiverChildConstraint);
            TR_ASSERT(stringLocation, "Expecting non null pointer to String object for constString or known String object");
            int32_t hashCode;
            bool success = comp()->fej9()->getStringHashCode(comp(), stringLocation, hashCode);
            if (!success)
               {
               if (trace())
                  traceMsg(comp(), "Cannot get access to the String object, quit transforming String.hashCode\n");
               break;
               }
            transformCallToIconstInPlaceOrInDelayedTransformations(_curTree, hashCode, receiverChildGlobal, transformNonnativeMethodInPlace);
            TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
            return;
            }
         break;
         }
      case TR::java_lang_String_equals:
         {
         // Only constrain the call in the last run of vp to avoid adding the candidate twice if the call is inside a loop
         if (!lastTimeThrough())
            return;
         int32_t numChildren = node->getNumChildren();
         TR::Node *receiverChild = node->getChild(numChildren - 2);
         TR::Node *objectChild = node->getChild(numChildren - 1);
         bool receiverChildGlobal, objectChildGlobal;
         TR::VPConstraint *receiverChildConstraint = getConstraint(receiverChild, receiverChildGlobal);
         TR::VPConstraint *objectChildConstraint = getConstraint(objectChild, objectChildGlobal);
         if (isStringObject(receiverChildConstraint) == TR_yes
             && receiverChildConstraint->isNonNullObject()
             && objectChildConstraint)
            {
            // According to java doc, String.equals returns false when the argument object is null
            if (objectChildConstraint->isNullObject())
               {
               transformCallToIconstInPlaceOrInDelayedTransformations(_curTree, 0, receiverChildGlobal && objectChildGlobal, transformNonnativeMethodInPlace);
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
               return;
               }

            TR_YesNoMaybe isObjectString = isStringObject(objectChildConstraint);

            if (isObjectString == TR_maybe)
               {
               if (trace())
                  traceMsg(comp(), "Argument type is unknown, quit transforming String.equals on node %p\n", node);
               break;
               }
            else if (isObjectString == TR_no)
               {
               transformCallToIconstInPlaceOrInDelayedTransformations(_curTree, 0, receiverChildGlobal && objectChildGlobal, transformNonnativeMethodInPlace);
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
               return;
               }
            else // isObjectString == TR_yes
               {
               // The receiver and the argument object have to be const strings or objects known to be strings
               if (isKnownStringObject(receiverChildConstraint)
                   && isKnownStringObject(objectChildConstraint))
                  {
                  uintptr_t* receiverLocation = getObjectLocationFromConstraint(receiverChildConstraint);
                  uintptr_t* objectLocation = getObjectLocationFromConstraint(objectChildConstraint);
                  TR_ASSERT(receiverLocation && objectLocation, "Expecting non null pointer to String object for constString or known String object");

                  int32_t result = 0;
                  bool success = comp()->fej9()->stringEquals(comp(), receiverLocation, objectLocation, result);
                  if (!success)
                     {
                     if (trace())
                        traceMsg(comp(), "Does not have VM access, cannot tell whether %p and %p are equal\n", receiverChild, objectChild);
                     break;
                     }
                  transformCallToIconstInPlaceOrInDelayedTransformations(_curTree, result, receiverChildGlobal && objectChildGlobal, transformNonnativeMethodInPlace);
                  TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
                  return;
                  }
               }
            }
         break;
         }
      case TR::java_lang_String_length:
      // Only constrain the call in the last run of vp to avoid adding the candidate twice if the call is inside a loop
      if (!lastTimeThrough())
         return;
      // java/lang/String.lengthInternal is only used internally so can be folded without generating HCR guard.
      case TR::java_lang_String_lengthInternal:
         {
         TR::Node *receiverChild = node->getLastChild();
         bool receiverChildGlobal;
         TR::VPConstraint *receiverChildConstraint = getConstraint(receiverChild, receiverChildGlobal);
         // Receiver is a const string or known string object
         if (isKnownStringObject(receiverChildConstraint))
            {
            uintptr_t* stringLocation = getObjectLocationFromConstraint(receiverChildConstraint);
            TR_ASSERT(stringLocation, "Expecting non null pointer to String object for constString or known String object");
            int32_t len;
            {
            TR::VMAccessCriticalSection getStringlength(comp(),
                                                        TR::VMAccessCriticalSection::tryToAcquireVMAccess);
            if (!getStringlength.hasVMAccess())
               break;
            uintptr_t stringObject = comp()->fej9()->getStaticReferenceFieldAtAddress((uintptr_t)stringLocation);
            len = comp()->fej9()->getStringLength(stringObject);
            }
            // java/lang/String.lengthInternal is used internally and HCR guards can be skipped for calls to it.
            transformCallToIconstInPlaceOrInDelayedTransformations(_curTree, len, receiverChildGlobal, transformNonnativeMethodInPlace || rm == TR::java_lang_String_lengthInternal);
            TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
            return;
            }
         break;
         }
      case TR::java_lang_Class_getModifiersImpl:
         {
         TR::Node *classChild = node->getLastChild();
         bool classChildGlobal;
         TR::VPConstraint *classChildConstraint = getConstraint(classChild, classChildGlobal);
         if (classChildConstraint && classChildConstraint->isJavaLangClassObject() == TR_yes
             && classChildConstraint->isNonNullObject()
             && classChildConstraint->getClassType()
             && classChildConstraint->getClassType()->asFixedClass())
            {
            int32_t modifiersForClass = 0;
            bool success = comp()->fej9()->javaLangClassGetModifiersImpl(classChildConstraint->getClass(), modifiersForClass);
            if (!success)
               break;
            transformCallToIconstInPlaceOrInDelayedTransformations(_curTree, modifiersForClass, classChildGlobal, true);
            TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
            return;
            }
         break;
         }
      case TR::java_lang_Class_isAssignableFrom:
         {
         int32_t numChildren = node->getNumChildren();
         TR::Node *firstClassChild = node->getChild(numChildren-2);
         bool firstClassChildGlobal;
         TR::VPConstraint *firstClassChildConstraint = getConstraint(firstClassChild, firstClassChildGlobal);
         TR::Node *secondClassChild = node->getChild(numChildren-1);
         bool secondClassChildGlobal;
         TR::VPConstraint *secondClassChildConstraint = getConstraint(secondClassChild, secondClassChildGlobal);

         if (firstClassChildConstraint && firstClassChildConstraint->isJavaLangClassObject() == TR_yes
             && firstClassChildConstraint->isNonNullObject()
             && firstClassChildConstraint->getClassType()
             && firstClassChildConstraint->getClassType()->asFixedClass())
            {
            if (secondClassChildConstraint && secondClassChildConstraint->isJavaLangClassObject() == TR_yes
                && secondClassChildConstraint->isNonNullObject()   // NullPointerException is expected if the class is null
                && secondClassChildConstraint->getClassType())
               {
               TR_OpaqueClassBlock *firstClass = firstClassChildConstraint->getClass();
               TR_OpaqueClassBlock *secondClass = secondClassChildConstraint->getClass();
               int32_t assignable = 0;
               if (secondClassChildConstraint->getClassType()->asFixedClass())
                  {
                  TR_YesNoMaybe isInstanceOfResult = comp()->fej9()->isInstanceOf(secondClass, firstClass, true, true);
                  TR_ASSERT(isInstanceOfResult != TR_maybe, "Expecting TR_yes or TR_no to call isInstanceOf when two types are fixed");
                  if (isInstanceOfResult == TR_yes)
                     assignable = 1;
                  }
               else if (secondClassChildConstraint->getClassType()->asResolvedClass())
                  {
                  TR_ASSERT_FATAL(firstClass != NULL && secondClass != NULL, "isAssignableFrom replacement at compile-time requires two class pointers - got %p and %p", firstClass, secondClass);
                  TR_YesNoMaybe isInstanceOfResult = comp()->fej9()->isInstanceOf(secondClass, firstClass, false, true);
                  if (isInstanceOfResult == TR_maybe)
                     {
                     if (trace())
                        traceMsg(comp(), "Relationship between %p and %p is unknown, quit transforming Class.isAssignableFrom\n", firstClassChild, secondClassChild);
                     return;
                     }
                  else if (isInstanceOfResult == TR_yes)
                     assignable = 1;
                  }
               else
                  {
                  if (trace())
                     traceMsg(comp(), "The second child class type is not resolved at compile-time, quit transforming Class.isAssignableFrom\n");
                  return;
                  }
               transformCallToIconstInPlaceOrInDelayedTransformations(_curTree, assignable, firstClassChildGlobal && secondClassChildGlobal, true);
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
               return;
               }
            }
         break;
         }
      case TR::java_lang_J9VMInternals_identityHashCode:
      case TR::java_lang_J9VMInternals_fastIdentityHashCode:
         {
         // Get the last child because sometimes the first child can be a pointer to java/lang/Class field in J9Class for a direct native call
         TR::Node *classChild = node->getLastChild();
         bool classChildGlobal;
         TR::VPConstraint *classChildConstraint = getConstraint(classChild, classChildGlobal);
         if (classChildConstraint && classChildConstraint->isJavaLangClassObject() == TR_yes
             && classChildConstraint->isNonNullObject()
             && classChildConstraint->getClassType()
             && classChildConstraint->getClassType()->asFixedClass())
            {
            bool hashCodeWasComputed = false;
            int32_t hashCodeForClass = comp()->fej9()->getJavaLangClassHashCode(comp(), classChildConstraint->getClass(), hashCodeWasComputed);
            if (hashCodeWasComputed)
               {
               transformCallToIconstInPlaceOrInDelayedTransformations(_curTree, hashCodeForClass, classChildGlobal, true);
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
               return;
               }
            }
         break;
         }
      case TR::java_lang_Class_getComponentType:
         {
         TR::Node *classChild = node->getLastChild();
         bool classChildGlobal;
         TR::VPConstraint *classChildConstraint = getConstraint(classChild, classChildGlobal);
         if (classChildConstraint && classChildConstraint->isJavaLangClassObject() == TR_yes
             && classChildConstraint->isNonNullObject()
             && classChildConstraint->getClassType()
             && classChildConstraint->getClassType()->asFixedClass())
            {
            TR_OpaqueClassBlock *thisClass = classChildConstraint->getClass();
            if (comp()->fej9()->isClassArray(thisClass))
               {
               TR_OpaqueClassBlock *arrayComponentClass = comp()->fej9()->getComponentClassFromArrayClass(thisClass);
               // J9Class pointer introduced by the opt has to be remembered under AOT
               if (!arrayComponentClass)
                  {
                  if (trace())
                     traceMsg(comp(), "Array component class cannot be remembered, quit transforming Class.getComponentType on node %p\n", node);
                  break;
                  }

               if (!performTransformation(comp(), "%sTransforming %s on node %p to an aloadi\n", OPT_DETAILS, signature, node))
                  break;

               anchorAllChildren(node, _curTree);
               node->removeAllChildren();

               // Use aconst instead of loadaddr because AOT relocation is not supported for loadaddr
               TR::Node *arrayComponentClassPointer = TR::Node::aconst(node, (uintptr_t)arrayComponentClass);
               // The classPointerConstant flag has to be set for AOT relocation
               arrayComponentClassPointer->setIsClassPointerConstant(true);
               node = TR::Node::recreateWithoutProperties(node, TR::aloadi, 1, arrayComponentClassPointer, comp()->getSymRefTab()->findOrCreateJavaLangClassFromClassSymbolRef());

               TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
               if (knot)
                  {
                  TR::KnownObjectTable::Index knownObjectIndex = knot->getOrCreateIndexAt((uintptr_t*)(arrayComponentClass + comp()->fej9()->getOffsetOfJavaLangClassFromClassField()));
                  addBlockOrGlobalConstraint(node,
                        TR::VPClass::create(this,
                           TR::VPKnownObject::createForJavaLangClass(this, knownObjectIndex),
                           TR::VPNonNullObject::create(this), NULL, NULL,
                           TR::VPObjectLocation::create(this, TR::VPObjectLocation::JavaLangClassObject)),
                        classChildGlobal);
                  }

               invalidateUseDefInfo();
               invalidateValueNumberInfo();
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
               return;
               }
            else
               {
               // Not an array, return null as the component type
               if (trace())
                  traceMsg(comp(), "Class is not an array class, transforming %s on node %p to null\n", signature, node);
               TR::VPConstraint *getComponentTypeConstraint = TR::VPNullObject::create(this);
               replaceByConstant(node, getComponentTypeConstraint, classChildGlobal);
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
               return;
               }
            }
         break;
         }
      case TR::java_lang_J9VMInternals_getSuperclass:
      case TR::com_ibm_jit_JITHelpers_getSuperclass:
         {
         TR::Node *classChild = node->getLastChild();
         bool classChildGlobal;
         TR::VPConstraint *classChildConstraint = getConstraint(classChild, classChildGlobal);
         if (classChildConstraint && classChildConstraint->isJavaLangClassObject() == TR_yes
             && classChildConstraint->isNonNullObject()
             && classChildConstraint->getClassType()
             && classChildConstraint->getClassType()->asFixedClass())
            {
            TR_OpaqueClassBlock * thisClass = classChildConstraint->getClass();
            // superClass is null for java/lang/Object class
            TR_OpaqueClassBlock *superClass = comp()->fej9()->getSuperClass(thisClass);

            // Super class does not exist for interface class, primitive class and java/lang/Object class object
            if (!superClass || TR::Compiler->cls.isInterfaceClass(comp(),thisClass) || TR::Compiler->cls.isPrimitiveClass(comp(), thisClass))
               {
               if (trace())
                  {
                  if (TR::Compiler->cls.isInterfaceClass(comp(),thisClass))
                     traceMsg(comp(), "Node %p is an interface class and does not have a super class\n", classChild);
                  else if (TR::Compiler->cls.isPrimitiveClass(comp(),thisClass))
                     traceMsg(comp(), "Node %p is a primitive class and does not have a super class\n", classChild);
                  else if (!superClass)
                     traceMsg(comp(), "Node %p is java/lang/Object class and does not have a super class\n", classChild);
                  traceMsg(comp(), "Transform call to %s on node %p to null\n", signature, node);
                  }
               TR::VPConstraint *getSuperClassConstraint = TR::VPNullObject::create(this);
               replaceByConstant(node, getSuperClassConstraint, classChildGlobal);
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
               return;
               }
            else
               {
               if (!performTransformation(comp(), "%sTransforming %s on node %p into an aloadi\n", OPT_DETAILS, signature, node))
                  break;

               anchorAllChildren(node, _curTree);
               node->removeAllChildren();

               // Use aconst instead of loadaddr because AOT relocation is not supported for loadaddr
               TR::Node *superClassPointer = TR::Node::aconst(node, (uintptr_t)superClass);
               // The classPointerConstant flag has to be set for AOT relocation
               superClassPointer->setIsClassPointerConstant(true);
               node = TR::Node::recreateWithoutProperties(node, TR::aloadi, 1, superClassPointer, comp()->getSymRefTab()->findOrCreateJavaLangClassFromClassSymbolRef());

               TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
               if (knot)
                  {
                  TR::KnownObjectTable::Index knownObjectIndex = knot->getOrCreateIndexAt((uintptr_t*)(superClass + comp()->fej9()->getOffsetOfJavaLangClassFromClassField()));
                  addBlockOrGlobalConstraint(node,
                        TR::VPClass::create(this,
                           TR::VPKnownObject::createForJavaLangClass(this, knownObjectIndex),
                           TR::VPNonNullObject::create(this), NULL, NULL,
                           TR::VPObjectLocation::create(this, TR::VPObjectLocation::JavaLangClassObject)),
                        classChildGlobal);
                  }

               invalidateUseDefInfo();
               invalidateValueNumberInfo();
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
               return;
               }
            }
         break;
         }
      }

   // The following are opts that do not support AOT
   if (!comp()->compileRelocatableCode())
      {
      switch (rm)
         {
         case TR::java_lang_invoke_MethodHandle_asType:
            {
            TR::Node* mh = node->getArgument(0);
            TR::Node* mt = node->getArgument(1);
            bool mhConstraintGlobal, mtConstraintGlobal;
            TR::VPConstraint* mhConstraint = getConstraint(mh, mhConstraintGlobal);
            TR::VPConstraint* mtConstraint = getConstraint(mt, mtConstraintGlobal);
            J9VMThread* vmThread = comp()->fej9()->vmThread();
            TR_OpaqueClassBlock* methodHandleClass = J9JitMemory::convertClassPtrToClassOffset(J9VMJAVALANGINVOKEMETHODHANDLE(vmThread->javaVM));
            TR_OpaqueClassBlock* methodTypeClass = J9JitMemory::convertClassPtrToClassOffset(J9VMJAVALANGINVOKEMETHODTYPE(vmThread->javaVM));
            if (mhConstraint
                && mtConstraint
                && mhConstraint->getKnownObject()
                && mhConstraint->isNonNullObject()
                && mtConstraint->getKnownObject()
                && mtConstraint->isNonNullObject()
                && mhConstraint->getClass()
                && mtConstraint->getClass()
                && comp()->fej9()->isInstanceOf(mhConstraint->getClass(), methodHandleClass, true, true) == TR_yes
                && comp()->fej9()->isInstanceOf(mtConstraint->getClass(), methodTypeClass, true, true) == TR_yes)
               {
               uintptr_t* mhLocation = getObjectLocationFromConstraint(mhConstraint);
               uintptr_t* mtLocation = getObjectLocationFromConstraint(mtConstraint);
               uint32_t mtOffset = J9VMJAVALANGINVOKEMETHODHANDLE_TYPE_OFFSET(vmThread);

               TR::VMAccessCriticalSection asType(comp(),
                                                           TR::VMAccessCriticalSection::tryToAcquireVMAccess);
               if (!asType.hasVMAccess())
                  break;
               uintptr_t mhObject = comp()->fej9()->getStaticReferenceFieldAtAddress((uintptr_t)mhLocation);
               uintptr_t mtInMh = comp()->fej9()->getReferenceFieldAtAddress(mhObject + mtOffset);
               uintptr_t mtObject = comp()->fej9()->getStaticReferenceFieldAtAddress((uintptr_t)mtLocation);

               if (trace())
                  traceMsg(comp(), "mhLocation %p mtLocation %p mhObject %p mtOffset %d mtInMh %p mtObject %p\n", mhLocation, mtLocation, mhObject, mtOffset, mtInMh, mtObject);

               // AOT doesn't relocate/validate objects, which is why this transformation is disabled under AOT
               if (mtInMh == mtObject)
                  {
                  if (!performTransformation(comp(), "%sTransforming %s on node %p to a PassThrough with child %p\n", OPT_DETAILS, signature, node, mh))
                     break;

                  TR::TransformUtil::transformCallNodeToPassThrough(this, node, _curTree, mh);
                  addGlobalConstraint(node, mhConstraint);
                  TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
                  return;
                  }
               }
            break;
            }
         case TR::java_lang_invoke_PrimitiveHandle_initializeClassIfRequired:
            {
            TR::Node* mh = node->getArgument(0);
            bool mhConstraintGlobal;
            TR::VPConstraint* mhConstraint = getConstraint(mh, mhConstraintGlobal);
            J9VMThread* vmThread = comp()->fej9()->vmThread();
            TR_OpaqueClassBlock* primitiveHandleClass = J9JitMemory::convertClassPtrToClassOffset(J9VMJAVALANGINVOKEPRIMITIVEHANDLE(vmThread->javaVM));
            bool removeCall = false;
            if (mhConstraint
                && mhConstraint->getKnownObject()
                && mhConstraint->isNonNullObject()
                && mhConstraint->getClass()
                && comp()->fej9()->isInstanceOf(mhConstraint->getClass(), primitiveHandleClass, true, true) == TR_yes)
               {
               uint32_t defcOffset = J9VMJAVALANGINVOKEPRIMITIVEHANDLE_DEFC_OFFSET(comp()->fej9()->vmThread());
               uintptr_t* mhLocation = getObjectLocationFromConstraint(mhConstraint);

               TR::VMAccessCriticalSection initializeClassIfRequired(comp(),
                                                        TR::VMAccessCriticalSection::tryToAcquireVMAccess);
               if (!initializeClassIfRequired.hasVMAccess())
                  break;
               uintptr_t mhObject = comp()->fej9()->getStaticReferenceFieldAtAddress((uintptr_t)mhLocation);
               uintptr_t defc = comp()->fej9()->getReferenceFieldAtAddress(mhObject + defcOffset);
               J9Class* defcClazz = (J9Class*)TR::Compiler->cls.classFromJavaLangClass(comp(), defc);
               if (defcClazz->initializeStatus == J9ClassInitSucceeded)
                  {
                  removeCall = true;
                  if (trace())
                     {
                     traceMsg(comp(), "defc in MethodHandle %p has been initialized\n", mh);
                     }
                  }
               }

            if (removeCall
                && performTransformation(comp(), "%sRemoving call node %s [" POINTER_PRINTF_FORMAT "]\n", OPT_DETAILS, node->getOpCode().getName(), node))
               {
               TR::Node* receiverChild = node->getArgument(0);
               TR::TransformUtil::transformCallNodeToPassThrough(this, node, _curTree, receiverChild);
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
               return;
               }
            break;
            }
         case TR::java_lang_invoke_DirectHandle_nullCheckIfRequired:
            {
            TR::Node* mh = node->getArgument(0);
            TR::Node* receiver = node->getArgument(1);
            bool mhConstraintGlobal, receiverConstraintGlobal;
            TR::VPConstraint* mhConstraint = getConstraint(mh, mhConstraintGlobal);
            TR::VPConstraint* receiverConstraint = getConstraint(receiver, receiverConstraintGlobal);

            J9VMThread* vmThread = comp()->fej9()->vmThread();
            TR_OpaqueClassBlock* directHandleClass = calledMethod->classOfMethod();
            if (!mhConstraint
                || !mhConstraint->getClass()
                || comp()->fej9()->isInstanceOf(mhConstraint->getClass(), directHandleClass, true, true) != TR_yes)
               break;

            bool removeCall = false;
            if (receiverConstraint
                && receiverConstraint->isNonNullObject())
               {
               removeCall = true;
               if (trace())
                  {
                  traceMsg(comp(), "receiver child %p for DirectHandle.nullCheckIfRequired is non-null\n", receiver);
                  }
               }
            else if (mhConstraint->getKnownObject()
                     && mhConstraint->isNonNullObject())
               {
               TR_OpaqueClassBlock* mhClass = mhConstraint->getClass();
               int32_t isStaticOffset = comp()->fej9()->getInstanceFieldOffset(mhClass, "isStatic", "Z");
               TR_ASSERT(isStaticOffset >= 0, "Can't find field isStatic in MethodHandle %p\n", mh);

               uintptr_t* mhLocation = getObjectLocationFromConstraint(mhConstraint);

               TR::VMAccessCriticalSection nullCheckIfRequired(comp(),
                                                        TR::VMAccessCriticalSection::tryToAcquireVMAccess);
               if (!nullCheckIfRequired.hasVMAccess())
                  break;
               uintptr_t mhObject = comp()->fej9()->getStaticReferenceFieldAtAddress((uintptr_t)mhLocation);
               int32_t isStatic = comp()->fej9()->getInt32FieldAt(mhObject, isStaticOffset);
               if (isStatic)
                  {
                  removeCall = true;
                  if (trace())
                     {
                     traceMsg(comp(), "DirectHandle %p refers to a static method\n", mh);
                     }
                  }
               else
                  {
                  // NULLCHK is required, insert a NULLCHK on the receiver object
                  dumpOptDetails(comp(), "%sInsert NULLCHK on node %s [" POINTER_PRINTF_FORMAT "] n%dn\n", OPT_DETAILS, node->getOpCode().getName(), node, node->getGlobalIndex());
                  TR::Node* passthrough = TR::Node::create(node, TR::PassThrough, 1, receiver);
                  TR::ResolvedMethodSymbol* owningMethodSymbol = node->getSymbolReference()->getOwningMethodSymbol(comp());
                  TR::Node* nullchk = TR::Node::createWithSymRef(node, TR::NULLCHK, 1, passthrough, comp()->getSymRefTab()->findOrCreateNullCheckSymbolRef(owningMethodSymbol));
                  _curTree->insertAfter(TR::TreeTop::create(comp(), nullchk));
                  removeCall = true;
                  }
               }

            if (removeCall
                && performTransformation(comp(), "%sRemoving call node %s [" POINTER_PRINTF_FORMAT "]\n", OPT_DETAILS, node->getOpCode().getName(), node))
               {
               TR::Node* receiverChild = node->getArgument(0);
               TR::TransformUtil::transformCallNodeToPassThrough(this, node, _curTree, receiverChild);
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
               return;
               }
            break;
            }
         // Transform java/lang/Object.newInstancePrototype into new and a call to default constructor of the given class
         // AOT class pointer relocation is only supported on aconst nodes. However aconst node cannot be a child work of
         // a TR::New node because it does not have a symref indicating the size of the instance of the class.
         case TR::java_lang_Object_newInstancePrototype:
            {
            // The result of newInstancePrototype will never be null since it's equivalent to bytecode new
            addGlobalConstraint(node, TR::VPNonNullObject::create(this));
            node->setIsNonNull(true);

            TR::Node *classChild = node->getChild(1);
            bool classChildGlobal;
            TR::VPConstraint *classChildConstraint = getConstraint(classChild, classChildGlobal);

            if (!classChildConstraint ||
                !classChildConstraint->isNonNullObject() ||
                classChildConstraint->isJavaLangClassObject() != TR_yes ||
                !classChildConstraint->getClassType() ||
                !classChildConstraint->getClassType()->asFixedClass())
               {
               if (trace())
                  traceMsg(comp(), "Class child %p is not a non-null java/lang/Class object with fixed class constraint, quit transforming Object.newInstancePrototype on node %p\n", classChild, node);
               break;
               }

            addGlobalConstraint(node, TR::VPFixedClass::create(this, classChildConstraint->getClass()));

            TR::Node *callerClassChild = node->getChild(2);
            bool callerClassChildGlobal;
            TR::VPConstraint *callerClassChildConstraint = getConstraint(callerClassChild, callerClassChildGlobal);
            if (!callerClassChildConstraint ||
                !callerClassChildConstraint->isNonNullObject() ||
                !callerClassChildConstraint->getClassType() ||
                !callerClassChildConstraint->getClassType()->asFixedClass() ||
                callerClassChildConstraint->isJavaLangClassObject() != TR_yes)
               {
               if (trace())
                  traceMsg(comp(), "Caller class %p is not a non-null java/lang/Class object with fixed class constraint, quit transforming Object.newInstancePrototype on node %p\n", callerClassChild, node);
               break;
               }

            TR_OpaqueClassBlock *newClass = classChildConstraint->getClass();
            TR_OpaqueClassBlock *callerClass = callerClassChildConstraint->getClass();

            // The following classes cannot be instantiated normally, i.e. via the new bytecode
            // InstantiationException will be thrown when calling java/lang/Class.newInstance on the following classes
            if (comp()->fej9()->isAbstractClass(newClass) ||
                comp()->fej9()->isPrimitiveClass(newClass) ||
                comp()->fej9()->isInterfaceClass(newClass) ||
                comp()->fej9()->isClassArray(newClass))
               {
               if (trace())
                  traceMsg(comp(), "Class is not instantiatable via bytecode new, quit transforming Object.newInstancePrototype on node %p\n", node);
               break;
               }

            // Visibility check
            if (!comp()->fej9()->isClassVisible(callerClass, newClass))
               {
               if (trace())
                  traceMsg(comp(), "Class is not visialbe to caller class, quit transforming Object.newInstancePrototype on node %p\n", node);
               break;
               }

            // Look up the default constructor for newClass, visibility check will be done during the look up
            TR_OpaqueMethodBlock *constructor = comp()->fej9()->getMethodFromClass(newClass, "<init>", "()V", callerClass);

            if (!constructor)
               {
               if (trace())
                  traceMsg(comp(), "Cannot find the default constructor, quit transforming Object.newInstancePrototype on node %p\n", node);
               break;
               }

            // Transform the call
            if (performTransformation(comp(), "%sTransforming %s to new and a call to constructor on node %p\n", OPT_DETAILS, signature, node))
               {
               anchorAllChildren(node, _curTree);
               node->removeAllChildren();

               TR::Node *classPointerNode = TR::Node::createWithSymRef(node, TR::loadaddr, 0,
                                         comp()->getSymRefTab()->findOrCreateClassSymbol(node->getSymbolReference()->getOwningMethodSymbol(comp()), -1, newClass));

               TR::Node::recreateWithoutProperties(node, TR::New, 1, comp()->getSymRefTab()->findOrCreateNewObjectSymbolRef(node->getSymbolReference()->getOwningMethodSymbol(comp())));
               node->setAndIncChild(0, classPointerNode);

               TR::SymbolReference *constructorSymRef = comp()->getSymRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1,
                  comp()->fej9()->createResolvedMethod(trMemory(), constructor), TR::MethodSymbol::Special);

               TR::TreeTop *constructorCall = TR::TreeTop::create(comp(), _curTree,
                   TR::Node::create(node, TR::treetop, 1,
                      TR::Node::createWithSymRef(node, TR::call, 1,
                         node,
                         constructorSymRef)));

               invalidateUseDefInfo();
               invalidateValueNumberInfo();
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "constrainCall/(%s)", signature));
               return;
               }
            break;
            }
         }
      }
   }

void
J9::ValuePropagation::doDelayedTransformations()
   {
   ListIterator<TreeIntResultPair> callsToBeFoldedToIconst(&_callsToBeFoldedToIconst);
   for (TreeIntResultPair *it = callsToBeFoldedToIconst.getFirst();
        it;
        it = callsToBeFoldedToIconst.getNext())
      {
      TR::TreeTop *callTree = it->_tree;
      int32_t result = it->_result;
      TR::Node * callNode = callTree->getNode()->getFirstChild();
      TR::Method * calledMethod = callNode->getSymbol()->castToMethodSymbol()->getMethod();
      const char *signature = calledMethod->signature(comp()->trMemory(), stackAlloc);

      if (!performTransformation(comp(), "%sTransforming call %s on node %p on tree %p to iconst %d\n", OPT_DETAILS, signature, callNode, callTree, result))
         break;

      transformCallToIconstWithHCRGuard(callTree, result);
      }
   _callsToBeFoldedToIconst.deleteAll();

   OMR::ValuePropagation::doDelayedTransformations();
   }



void
J9::ValuePropagation::getParmValues()
   {
   // Determine how many parms there are
   //
   TR::ResolvedMethodSymbol *methodSym = comp()->getMethodSymbol();
   int32_t numParms = methodSym->getParameterList().getSize();
   if (numParms == 0)
      return;

   // Allocate the constraints array for the parameters
   //
   _parmValues = (TR::VPConstraint**)trMemory()->allocateStackMemory(numParms*sizeof(TR::VPConstraint*));

   // Create a constraint for each parameter that we can find info for.
   // First look for a "this" parameter then look through the method's signature
   //
   TR_ResolvedMethod *method = comp()->getCurrentMethod();
   TR_OpaqueClassBlock *classObject;

   if (!chTableValidityChecked() && usePreexistence())
      {
      TR::ClassTableCriticalSection setCHTableWasValid(comp()->fe());
      if (comp()->getFailCHTableCommit())
         setChTableWasValid(false);
      else
         setChTableWasValid(true);
      setChTableValidityChecked(true);
      }

   int32_t parmIndex = 0;
   TR::VPConstraint *constraint = NULL;
   ListIterator<TR::ParameterSymbol> parms(&methodSym->getParameterList());
   TR::ParameterSymbol *p = parms.getFirst();
   if (!comp()->getCurrentMethod()->isStatic())
      {
      if (p && p->getOffset() == 0)
         {
         classObject = method->containingClass();

         TR_OpaqueClassBlock *prexClass = NULL;
         if (usePreexistence())
            {
            TR::ClassTableCriticalSection usesPreexistence(comp()->fe());

            prexClass = classObject;
            if (TR::Compiler->cls.isAbstractClass(comp(), classObject))
               classObject = comp()->getPersistentInfo()->getPersistentCHTable()->findSingleConcreteSubClass(classObject, comp());

            if (!classObject)
               {
               classObject = prexClass;
               prexClass = NULL;
               }
            else
               {
               TR_PersistentClassInfo * cl = comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(classObject, comp());
               if (!cl)
                  prexClass = NULL;
               else
                  {
                  if (!cl->shouldNotBeNewlyExtended())
                     _resetClassesThatShouldNotBeNewlyExtended.add(cl);
                  cl->setShouldNotBeNewlyExtended(comp()->getCompThreadID());

                  TR_ScratchList<TR_PersistentClassInfo> subClasses(trMemory());
                  TR_ClassQueries::collectAllSubClasses(cl, &subClasses, comp());

                  ListIterator<TR_PersistentClassInfo> it(&subClasses);
                  TR_PersistentClassInfo *info = NULL;
                  for (info = it.getFirst(); info; info = it.getNext())
                     {
                     if (!info->shouldNotBeNewlyExtended())
                        _resetClassesThatShouldNotBeNewlyExtended.add(info);
                     info->setShouldNotBeNewlyExtended(comp()->getCompThreadID());
                     }
                  }
               }

            }

         if (prexClass && !fe()->classHasBeenExtended(classObject))
            {
            TR_OpaqueClassBlock *jlKlass = fe()->getClassClassPointer(classObject);
            if (jlKlass)
               {
               if (classObject != jlKlass)
                  {
                  if (!fe()->classHasBeenExtended(classObject))
                     constraint = TR::VPFixedClass::create(this, classObject);
                  else
                     constraint = TR::VPResolvedClass::create(this, classObject);
                  }
               else
                  constraint = TR::VPObjectLocation::create(this, TR::VPObjectLocation::JavaLangClassObject);
               constraint = constraint->intersect(TR::VPPreexistentObject::create(this, prexClass), this);
               TR_ASSERT(constraint, "Cannot intersect constraints");
               }
            }
         else
            {
            // Constraining the receiver's type here should be fine, even if
            // its declared type is an interface (i.e. for a default method
            // implementation). The receiver must always be an instance of (a
            // subtype of) the type that declares the method.
            TR_OpaqueClassBlock *jlKlass = fe()->getClassClassPointer(classObject);
            if (jlKlass)
               {
               if (classObject != jlKlass)
                  constraint = TR::VPResolvedClass::create(this, classObject);
               else
                  constraint = TR::VPObjectLocation::create(this, TR::VPObjectLocation::JavaLangClassObject);
               }
            }

         if (0 && constraint) // cannot do this if 'this' is changed in the method...allow the non null property on the TR::Node set by IL gen to dictate the creation of non null constraint
            {
            constraint = constraint->intersect(TR::VPNonNullObject::create(this), this);
            TR_ASSERT(constraint, "Cannot intersect constraints");
            }

         _parmValues[parmIndex++] = constraint;
         p = parms.getNext();
         }
      }

   TR_MethodParameterIterator * parmIterator = method->getParameterIterator(*comp());
   for ( ; p; p = parms.getNext())
      {
      TR_ASSERT(!parmIterator->atEnd(), "Ran out of parameters unexpectedly.");
      TR::DataType dataType = parmIterator->getDataType();
      if ((dataType == TR::Int8 || dataType == TR::Int16)
          && comp()->getOption(TR_AllowVPRangeNarrowingBasedOnDeclaredType))
         {
         _parmValues[parmIndex++] = TR::VPIntRange::create(this, dataType, TR_maybe);
         }
      else if (dataType == TR::Aggregate)
         {
         constraint = NULL;

         TR_OpaqueClassBlock *opaqueClass = parmIterator->getOpaqueClass();
         if (opaqueClass)
            {
            TR_OpaqueClassBlock *prexClass = NULL;
            if (usePreexistence())
               {
               TR::ClassTableCriticalSection usesPreexistence(comp()->fe());

               prexClass = opaqueClass;
               if (TR::Compiler->cls.isInterfaceClass(comp(), opaqueClass) || TR::Compiler->cls.isAbstractClass(comp(), opaqueClass))
                  opaqueClass = comp()->getPersistentInfo()->getPersistentCHTable()->findSingleConcreteSubClass(opaqueClass, comp());

               if (!opaqueClass)
                  {
                  opaqueClass = prexClass;
                  prexClass = NULL;
                  }
               else
                  {
                  TR_PersistentClassInfo * cl = comp()->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(opaqueClass, comp());
                  if (!cl)
                     prexClass = NULL;
                  else
                     {
                     if (!cl->shouldNotBeNewlyExtended())
                        _resetClassesThatShouldNotBeNewlyExtended.add(cl);
                     cl->setShouldNotBeNewlyExtended(comp()->getCompThreadID());

                     TR_ScratchList<TR_PersistentClassInfo> subClasses(trMemory());
                     TR_ClassQueries::collectAllSubClasses(cl, &subClasses, comp());

                     ListIterator<TR_PersistentClassInfo> it(&subClasses);
                     TR_PersistentClassInfo *info = NULL;
                     for (info = it.getFirst(); info; info = it.getNext())
                        {
                        if (!info->shouldNotBeNewlyExtended())
                           _resetClassesThatShouldNotBeNewlyExtended.add(info);
                        info->setShouldNotBeNewlyExtended(comp()->getCompThreadID());
                        }
                     }
                  }

               }

            if (prexClass && !fe()->classHasBeenExtended(opaqueClass))
               {
               TR_OpaqueClassBlock *jlKlass = fe()->getClassClassPointer(opaqueClass);
               if (jlKlass)
                  {
                  if (opaqueClass != jlKlass)
                     {
                     if (!fe()->classHasBeenExtended(opaqueClass))
                        constraint = TR::VPFixedClass::create(this, opaqueClass);
                     else
                        constraint = TR::VPResolvedClass::create(this, opaqueClass);
                     }
                  else
                     constraint = TR::VPObjectLocation::create(this, TR::VPObjectLocation::JavaLangClassObject);
                  constraint = constraint->intersect(TR::VPPreexistentObject::create(this, prexClass), this);
                  TR_ASSERT(constraint, "Cannot intersect constraints");
                  }
               }
            else if (!TR::Compiler->cls.isInterfaceClass(comp(), opaqueClass)
                     || comp()->getOption(TR_TrustAllInterfaceTypeInfo))
               {
               // Interface-typed parameters are not handled here because they
               // will accept arbitrary objects.
               TR_OpaqueClassBlock *jlKlass = fe()->getClassClassPointer(opaqueClass);
               if (jlKlass)
                  {
                  if (opaqueClass != jlKlass)
                     constraint = TR::VPResolvedClass::create(this, opaqueClass);
                  else
                     constraint = TR::VPObjectLocation::create(this, TR::VPObjectLocation::JavaLangClassObject);
                  }
               }
            }
         else if (0) //added here since an unresolved parm could be an interface in which case nothing is known
            {
            char *sig;
            uint32_t len;
            sig = parmIterator->getUnresolvedJavaClassSignature(len);
            constraint = TR::VPUnresolvedClass::create(this, sig, len, method);
            if (usePreexistence() && parmIterator->isClass())
               {
               classObject = constraint->getClass();
               if (classObject && !fe()->classHasBeenExtended(classObject))
                  constraint = TR::VPFixedClass::create(this, classObject);
               constraint = constraint->intersect(TR::VPPreexistentObject::create(this, classObject), this);
               TR_ASSERT(constraint, "Cannot intersect constraints");
               }
            }

         _parmValues[parmIndex++] = constraint;
         }
      else
         {
         _parmValues[parmIndex++] = NULL;
         }
      parmIterator->advanceCursor();
      }

   TR_ASSERT(parmIterator->atEnd() && parmIndex == numParms, "Bad signature for owning method");
   }

static bool isStaticFinalFieldWorthFolding(TR::Compilation* comp, TR_OpaqueClassBlock* declaringClass, char* fieldSignature, int32_t fieldSigLength)
   {
   if (comp->getMethodSymbol()->hasMethodHandleInvokes()
       && !TR::Compiler->cls.classHasIllegalStaticFinalFieldModification(declaringClass))
      {
      if (fieldSigLength == 28 && !strncmp(fieldSignature, "Ljava/lang/invoke/VarHandle;", 28))
         return true;
      }

   return false;
   }

bool J9::ValuePropagation::transformDirectLoad(TR::Node* node)
   {
   // Allow OMR to fold reliable static final field
   if (OMR::ValuePropagation::transformDirectLoad(node))
      return true;

   // Limited to varhandle folding only in VP
   if (node->isLoadOfStaticFinalField() &&
          TR::TransformUtil::attemptVarHandleStaticFinalFieldFolding(this, _curTree, node))
      {
      return true;
      }

   return false;
   }

static void getHelperSymRefs(OMR::ValuePropagation *vp, TR::Node *curCallNode, TR::SymbolReference *&getHelpersSymRef, TR::SymbolReference *&helperSymRef, char *helperSig, int32_t helperSigLen, TR::MethodSymbol::Kinds helperCallKind)
   {
   //Function to retrieve the JITHelpers.getHelpers and JITHelpers.<helperSig> method symbol references.
   //
   TR_OpaqueClassBlock *jitHelpersClass = vp->comp()->getJITHelpersClassPointer();

   //If we can't find the helper class, or it isn't initalized, return.
   //
   if (!jitHelpersClass || !TR::Compiler->cls.isClassInitialized(vp->comp(), jitHelpersClass))
      return;

   TR_ScratchList<TR_ResolvedMethod> helperMethods(vp->trMemory());
   vp->comp()->fej9()->getResolvedMethods(vp->trMemory(), jitHelpersClass, &helperMethods);
   ListIterator<TR_ResolvedMethod> it(&helperMethods);

   //Find the symRefs
   //
   for (TR_ResolvedMethod *m = it.getCurrent(); m; m = it.getNext())
      {
      char *sig = m->nameChars();
      //printf("Here is the sig %s and the passed in %s \n", sig,helperSig);
      if (!strncmp(sig, helperSig, helperSigLen))
         {
         if (TR::MethodSymbol::Virtual == helperCallKind)
            {
            //REVISIT FOR IMPL HASH CODE***
            //
            helperSymRef = vp->comp()->getSymRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, m, TR::MethodSymbol::Virtual);
            helperSymRef->setOffset(TR::Compiler->cls.vTableSlot(vp->comp(), m->getPersistentIdentifier(), jitHelpersClass));
            }
         else
            {
            helperSymRef = vp->comp()->getSymRefTab()->findOrCreateMethodSymbol(curCallNode->getSymbolReference()->getOwningMethodIndex(), -1, m, helperCallKind);
            }
         //printf("found gethelpers, 0x%x \n", helperSymRef);
         }
      else if (!strncmp(sig, "jitHelpers", 10))
         {
         //printf("found helpers");
         getHelpersSymRef = vp->comp()->getSymRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, m, TR::MethodSymbol::Static);
         }
      }
   }


static void transformToOptimizedCloneCall(OMR::ValuePropagation *vp, TR::Node *node, bool isDirectCall)
   {
   TR::SymbolReference *getHelpersSymRef = NULL;
   TR::SymbolReference *optimizedCloneSymRef = NULL;

   getHelperSymRefs(vp, node, getHelpersSymRef, optimizedCloneSymRef, "optimizedClone", 14, TR::MethodSymbol::Special);

   //printf("helper sym 0x%x, optsym 0x%x \n", getHelpersSymRef, optimizedCloneSymRef);

   if (optimizedCloneSymRef && getHelpersSymRef && performTransformation(vp->comp(), "%sChanging call to new optimizedClone at node [%p]\n", OPT_DETAILS, node))
        {
        //FIXME: add me to the list of calls to be inlined
        //
        TR::Method *method = optimizedCloneSymRef->getSymbol()->castToMethodSymbol()->getMethod();
        TR::Node *helpersCallNode = TR::Node::createWithSymRef(node, method->directCallOpCode(), 0, getHelpersSymRef);
        TR::TreeTop *helpersCallTT = TR::TreeTop::create(vp->comp(), TR::Node::create(TR::treetop, 1, helpersCallNode));
        vp->_curTree->insertBefore(helpersCallTT);

        method = optimizedCloneSymRef->getSymbol()->castToMethodSymbol()->getMethod();
        TR::Node::recreate(node, method->directCallOpCode());
        TR::Node *firstChild = node->getFirstChild();
        firstChild->decReferenceCount();
        node->setNumChildren(2);
        node->setAndIncChild(0, helpersCallNode);
        node->setAndIncChild(1, firstChild);
        node->setSymbolReference(optimizedCloneSymRef);
        vp->invalidateUseDefInfo();
        vp->invalidateValueNumberInfo();
        }
//printf("TRANSFORMED \n");
   }


static TR::Node *setCloneClassInNode(OMR::ValuePropagation *vp, TR::Node *node, TR::VPConstraint *constraint, bool isGlobal)
   {

   // If the child is a resolved class type, hide the class pointer in the
   // second child
   //

   if(!node->isProcessedByCallCloneConstrain())
      {
      node->setSecond(NULL);
      node->setProcessedByCallCloneConstrain();
      }

   if (constraint && constraint->getClass())
      {
      TR_OpaqueClassBlock *clazz = constraint->getClass();
      if (constraint->isClassObject() == TR_yes)
         clazz = vp->fe()->getClassClassPointer(clazz);

      if (clazz && (TR::Compiler->cls.classDepthOf(clazz) == 0) &&
          !constraint->isFixedClass())
         clazz = NULL;

      if (node->getCloneClassInNode() &&
          clazz &&
          (node->getCloneClassInNode() != clazz))
         {
         TR_YesNoMaybe answer = vp->fe()->isInstanceOf(clazz, node->getCloneClassInNode(), true, true);
         if (answer != TR_yes)
            clazz = node->getCloneClassInNode();
         }
      if (performTransformation(vp->comp(), "%sSetting type on Object.Clone acall node [%p] to [%p]\n", OPT_DETAILS, node, clazz))
         node->setSecond((TR::Node*)clazz);
      }
   return node;
   }


TR::Node *
J9::ValuePropagation::innerConstrainAcall(TR::Node *node)
   {
   // This node can be constrained by the return type of the method.
   //
   TR::VPConstraint *constraint = NULL;
   TR::SymbolReference * symRef = node->getSymbolReference();
   TR::ResolvedMethodSymbol *method = symRef->getSymbol()->getResolvedMethodSymbol();

   // For the special case of a direct call to Object.clone() the return type
   // will be the same as the type of the "this" argument, which may be more
   // precise than the declared return type of "Object".
   //
   if (method)
      {
      if (!node->getOpCode().isIndirect())
         {
         static char *enableDynamicObjectClone = feGetEnv("TR_enableDynamicObjectClone");
         // Dynamic cloning kicks in when we attempt to make direct call to Object.clone
         // or J9VMInternals.primitiveClone where the cloned object is an array.
         if (method->getRecognizedMethod() == TR::java_lang_Object_clone
             || method->getRecognizedMethod() == TR::java_lang_J9VMInternals_primitiveClone)
            {
            bool isGlobal;
            if (method->getRecognizedMethod() == TR::java_lang_Object_clone)
              constraint = getConstraint(node->getFirstChild(), isGlobal);
            else
              constraint = getConstraint(node->getLastChild(), isGlobal);

            TR::VPResolvedClass *newTypeConstraint = NULL;
            if (constraint)
               {
               // Do nothing if the class of the object doesn't implement Cloneable
               if (constraint->getClass() && !comp()->fej9()->isCloneable(constraint->getClass()))
                  {
                  if (trace())
                     traceMsg(comp(), "Object Clone: Class of node %p is not cloneable, quit\n", node);

                  TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "inlineClone/unsuitable/(%s)/%s/block_%d", comp()->signature(), comp()->getHotnessName(comp()->getMethodHotness()), _curTree->getEnclosingBlock()->getNumber()));

                  return node;
                  }
               if ( constraint->isFixedClass() )
                  {
                  newTypeConstraint = TR::VPFixedClass::create(this, constraint->getClass());

                  if (!comp()->compileRelocatableCode())
                     {
                     if (constraint->getClassType()
                         && constraint->getClassType()->isArray() == TR_no
                         && !_objectCloneCalls.find(_curTree))
                        {
                        _objectCloneCalls.add(_curTree);
                        _objectCloneTypes.add(new (trStackMemory()) OMR::ValuePropagation::ObjCloneInfo(constraint->getClass(), true));
                        }
                     else if (constraint->getClassType()
                              && constraint->getClassType()->isArray() == TR_yes
                              && !_arrayCloneCalls.find(_curTree))
                        {
                        _arrayCloneCalls.add(_curTree);
                        _arrayCloneTypes.add(new (trStackMemory()) OMR::ValuePropagation::ArrayCloneInfo(constraint->getClass(), true));
                        }
                     }
                  }
               // Dynamic object clone is enabled only with FLAGS_IN_CLASS_SLOT enabled
               // as currenty codegen anewarray evaluator only supports this case for object header initialization.
               // Even though all existing supported build config have this flag set, this ifdef serves as a safety precaution.
#if defined(J9VM_INTERP_FLAGS_IN_CLASS_SLOT)
               else if ( constraint->getClassType()
                         && constraint->getClassType()->asResolvedClass() )
                  {
                  newTypeConstraint = TR::VPResolvedClass::create(this, constraint->getClass());
                  if (trace())
                     traceMsg(comp(), "Object Clone: Resolved Class of node %p \n", node);
                  if (enableDynamicObjectClone
                      && constraint->getClassType()->isArray() == TR_no
                      && !_objectCloneCalls.find(_curTree))
                     {
                     if (trace())
                        traceMsg(comp(), "Object Clone: Resolved Class of node %p object clone\n", node);
                     _objectCloneCalls.add(_curTree);
                     _objectCloneTypes.add(new (trStackMemory()) OMR::ValuePropagation::ObjCloneInfo(constraint->getClass(), false));
                     }
                  // Currently enabled for X86 as the required codegen support is implemented on X86 only.
                  // Remove the condition as other platforms receive support.
                  else if (comp()->cg()->getSupportsDynamicANewArray()
                      && constraint->getClassType()->isArray() == TR_yes
                      && !_arrayCloneCalls.find(_curTree)
                      && !comp()->generateArraylets())
                     {
                     if (trace())
                        traceMsg(comp(), "Object Clone: Resolved Class of node %p array clone\n", node);
                     _arrayCloneCalls.add(_curTree);
                     _arrayCloneTypes.add(new (trStackMemory()) OMR::ValuePropagation::ArrayCloneInfo(constraint->getClass(), false));;
                     }
                  }
#endif
               }

            if (!constraint || (!constraint->isFixedClass()
                && (enableDynamicObjectClone && !(constraint->getClassType() && constraint->getClassType()->asResolvedClass() && constraint->getClassType()->isArray() == TR_no))))
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "inlineClone/miss/(%s)/%s/block_%d", comp()->signature(), comp()->getHotnessName(comp()->getMethodHotness()), _curTree->getEnclosingBlock()->getNumber()));
            else
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "inlineClone/hit/(%s)/%s/block_%d", comp()->signature(), comp()->getHotnessName(comp()->getMethodHotness()), _curTree->getEnclosingBlock()->getNumber()));

            TR::VPClassPresence *cloneResultNonNull = TR::VPNonNullObject::create(this);
            TR::VPObjectLocation *cloneResultOnHeap = TR::VPObjectLocation::create(this, TR::VPObjectLocation::HeapObject);
            TR::VPArrayInfo *cloneResultArrayInfo = NULL;
            if (constraint)
               cloneResultArrayInfo = constraint->getArrayInfo();
            TR::VPConstraint *newConstraint = TR::VPClass::create(this, newTypeConstraint, cloneResultNonNull, NULL, cloneResultArrayInfo, cloneResultOnHeap);

            // This constraint can be global because the result of the clone call
            // needs to have its own value number.
            addGlobalConstraint(node, newConstraint);

            if (method->getRecognizedMethod() == TR::java_lang_Object_clone
                && (constraint && !constraint->isFixedClass()))
               node = setCloneClassInNode(this, node, newConstraint, isGlobal);

            // OptimizedClone
            if(comp()->getOption(TR_EnableJITHelpersoptimizedClone) && newTypeConstraint)
               transformToOptimizedCloneCall(this, node, true);
            return node;
            }
         else if (method->getRecognizedMethod() == TR::java_math_BigDecimal_valueOf)
            {
            TR_ResolvedMethod *owningMethod = symRef->getOwningMethod(comp());
            TR_OpaqueClassBlock *classObject = fe()->getClassFromSignature("java/math/BigDecimal", 20, owningMethod);
            if (classObject)
               {
               constraint = TR::VPFixedClass::create(this, classObject);
               addGlobalConstraint(node, constraint);
               addGlobalConstraint(node, TR::VPNonNullObject::create(this));
               }
            }
         }
      else
         {
         if ((method->getRecognizedMethod() == TR::java_math_BigDecimal_add) ||
             (method->getRecognizedMethod() == TR::java_math_BigDecimal_subtract) ||
             (method->getRecognizedMethod() == TR::java_math_BigDecimal_multiply))
            {
            bool isGlobal;
            constraint = getConstraint(node->getSecondChild(), isGlobal);
            TR_ResolvedMethod *owningMethod = symRef->getOwningMethod(comp());
            TR_OpaqueClassBlock * bigDecimalClass = fe()->getClassFromSignature("java/math/BigDecimal", 20, owningMethod);
            //traceMsg(comp(), "child %p big dec class %p\n", constraint, bigDecimalClass);
            if (constraint && bigDecimalClass &&
                constraint->isFixedClass() &&
                (bigDecimalClass == constraint->getClass()))
               {
               TR::VPConstraint *newConstraint = TR::VPFixedClass::create(this, bigDecimalClass);
               addBlockOrGlobalConstraint(node, newConstraint,isGlobal);
               addGlobalConstraint(node, TR::VPNonNullObject::create(this));
               return node;
               }
            }
         }
      }

   int32_t len = 0;
   const char * sig = symRef->getTypeSignature(len);

   if (sig == NULL)  // helper
       return node;

   TR_ASSERT(sig[0] == 'L' || sig[0] == '[', "Ref call return type is not a class");

   TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();
   TR_ResolvedMethod *owningMethod = symRef->getOwningMethod(comp());
   TR_OpaqueClassBlock *classBlock = fe()->getClassFromSignature(sig, len, owningMethod);
   if (  classBlock
      && TR::Compiler->cls.isInterfaceClass(comp(), classBlock)
      && !comp()->getOption(TR_TrustAllInterfaceTypeInfo))
      {
      // Can't trust interface type info coming from method return value
      classBlock = NULL;
      }
   if (classBlock)
      {
      TR_OpaqueClassBlock *jlClass = fe()->getClassClassPointer(classBlock);
      if (jlClass)
         {
         if (classBlock != jlClass)
            constraint = TR::VPClassType::create(this, sig, len, owningMethod, false, classBlock);
         else
            constraint = TR::VPObjectLocation::create(this, TR::VPObjectLocation::JavaLangClassObject);
         addGlobalConstraint(node, constraint);
         }
      }
   else if (symRef->isUnresolved() && symbol && !symbol->isInterface())
      {
      TR::VPConstraint *constraint = TR::VPUnresolvedClass::create(this, sig, len, owningMethod);
      addGlobalConstraint(node, constraint);
      }

   return node;
   }
