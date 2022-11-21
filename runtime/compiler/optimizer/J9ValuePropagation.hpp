/*******************************************************************************
 * Copyright (c) 2000, 2022 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef J9_VALUEPROPAGATION_INCL
#define J9_VALUEPROPAGATION_INCL

#include "optimizer/OMRValuePropagation.hpp"
#include "infra/List.hpp"

namespace TR { class VP_BCDSign; }

namespace J9
{

class ValuePropagation : public OMR::ValuePropagation
   {
   public:

   ValuePropagation(TR::OptimizationManager *manager);

 #if (defined(LINUX) && ( defined(TR_TARGET_X86) || defined(TR_TARGET_S390)))
  #if __GNUC__ > 4 || \
   (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
   TR::VP_BCDSign **getBCDSignConstraints(TR::DataType dt) __attribute__((optimize(1)));
  #else
   TR::VP_BCDSign **getBCDSignConstraints(TR::DataType dt);
  #endif
 #else
   TR::VP_BCDSign **getBCDSignConstraints(TR::DataType dt);
 #endif

   virtual void constrainRecognizedMethod(TR::Node *node);
   virtual bool transformUnsafeCopyMemoryCall(TR::Node *arrayCopyNode);
   virtual bool transformDirectLoad(TR::Node *node);
   virtual bool isUnreliableSignatureType(
      TR_OpaqueClassBlock *klass, TR_OpaqueClassBlock *&erased);
   virtual void doDelayedTransformations();
   void transformCallToNodeWithHCRGuard(TR::TreeTop *callTree, TR::Node *result);
   void transformCallToIconstInPlaceOrInDelayedTransformations(TR::TreeTop *callTree, int32_t result, bool isGlobal, bool inPlace = true, bool requiresGuard = false);
   void transformCallToNodeDelayedTransformations(TR::TreeTop *callTree, TR::Node *result, bool requiresGuard = false);
   uintptr_t* getObjectLocationFromConstraint(TR::VPConstraint *constraint);
   bool isKnownStringObject(TR::VPConstraint *constraint);
   TR_YesNoMaybe isStringObject(TR::VPConstraint *constraint);

   /**
    * Determine whether the component type of an array is, or might be, a primitive value
    * type.
    * \param arrayConstraint The \ref TR::VPConstraint type constraint for the array reference
    * \returns \c TR_yes if the array's component type is definitely a primitive value type;\n
    *          \c TR_no if it is definitely not a primitive value type; or\n
    *          \c TR_maybe otherwise.
    */
   virtual TR_YesNoMaybe isArrayCompTypePrimitiveValueType(TR::VPConstraint *arrayConstraint);

   /**
    * \brief
    *    Determines whether the array element is, or might be, flattened.
    *
    * \param arrayConstraint
    *    The \ref TR::VPConstraint type constraint for the array reference.
    *
    * \returns \c TR_yes if the array element is flattened;\n
    *          \c TR_no if it is definitely not flattened; or\n
    *          \c TR_maybe otherwise.
    */
   TR_YesNoMaybe isArrayElementFlattened(TR::VPConstraint *arrayConstraint);

   /**
    * \brief
    *    Transforms jitLoadFlattenableArrayElement helper call to use sym refs to load the
    *    flattened array element.
    *
    * \param arrayClass
    *    The array class that contains the flattened array element.
    *
    * \param callNode
    *    The call node for jitLoadFlattenableArrayElement.
    *
    */
   void transformFlattenedArrayElementLoad(TR_OpaqueClassBlock *arrayClass, TR::Node *callNode);

   /**
    * \brief
    *    Transforms jitStoreFlattenableArrayElement helper call to use sym refs to store into the
    *    flattened array element. If the flattened array element has zero field, the call tree will
    *    be removed.
    *
    * \param arrayClass
    *    The array class that contains the flattened array element.
    *
    * \param callTree
    *    The call tree for jitStoreFlattenableArrayElement callNode.
    *
    * \param callNode
    *    The call node for jitStoreFlattenableArrayElement.
    *
    * \param needsNullValueCheck
    *    If a null check needs to be added on the value that is being store into the array.
    *
    * \return
    *    Return true if the call tree is removed because the flattened array element has zero field,
    *    false otherwise.
    *
    */
   bool transformFlattenedArrayElementStore(TR_OpaqueClassBlock *arrayClass, TR::TreeTop *callTree, TR::Node *callNode, bool needsNullValueCheck);

   /**
    * \brief
    *    Transforms object{Inequality|Equality}Comparison helper call if
    *    (1) field count is 0, fold the helper call into a constant
    *    (2) field count is 1 and the field is integral or identity class reference,
    *        transform the helper call to field comparison
    *    (3) field count > 1 and all fields meet direct memory comparison,
    *        transform the helper call to arraycmp
    *
    * \param containingClass
    *    The class that contains the fields
    *
    * \param callNode
    *    The call node for object{Inequality|Equality}Comparison
    *
    */
   void transformVTObjectEqNeCompare(TR_OpaqueClassBlock *containingClass, TR::Node *callNode);

   /**
    * Determine the bounds and element size for an array constraint
    *
    * \param[in] arrayConstraint A \ref TR::VPConstraint for an array reference
    * \param[out] lowerBoundLimit The lower bound on the size of the array
    * \param[out] upperBoundLimit The upper bound on the size of the array
    * \param[out] elementSize The size of an element of the array; zero if not known
    * \param[out] isKnownObj Set to \c true if this constraint represents a known object;\n
    *             \c false otherwise.
    */
   virtual void getArrayLengthLimits(TR::VPConstraint *arrayConstraint, int32_t &lowerBoundLimit, int32_t &upperBoundLimit,
                   int32_t &elementSize, bool &isKnownObj);


   virtual void getParmValues();

   /**
    * @brief Supplemental functionality for constraining an acall node.  Projects
    *        consuming OMR can implement this function to provide project-specific
    *        functionality.
    *
    * @param[in] node : TR::Node of the call to constrain
    *
    * @return Resulting node with constraints applied.
    */
   virtual TR::Node *innerConstrainAcall(TR::Node *node);

   private:

   /**
    * \brief
    *    Transforms a call to a String indexOf method when the source string is a
    *    KnownObject or ConstString.
    *
    * \parm indexOfNode
    *    Node corresponding to a call to an indexOf method.
    *
    * \parm sourceStringNode
    *    Node corresponding to the source string (the string to search through).
    *
    * \parm targetCharNode
    *    Node corresponding to the target character (the character to search for).
    *
    * \parm startNode
    *    The index in the source string at which to start searching from.
    *    If NULL, search will start at index zero.
    *
    * \parm lengthNode
    *    The length in characters of the source string.
    *    If lengthNode is NULL, will attempt to determine length from constraint
    *    on sourceStringNode.
    *
    * \parm is16Bit
    *    True if each character in the source string is 16 bits. If false, assumed
    *    to be 8 bits.
    *
    * \return
    *    Return true if a transformation was performed, false otherwise.
    */
   bool transformIndexOfKnownString(
      TR::Node *indexOfNode,
      TR::Node *sourceStringNode,
      TR::Node *targetCharNode,
      TR::Node *startNode,
      TR::Node *lengthNode,
      bool is16Bit = true);

   struct TreeNodeResultPair {
      TR_ALLOC(TR_Memory::ValuePropagation)
      TR::TreeTop *_tree;
      TR::Node *_result;
      bool _requiresHCRGuard;
      TreeNodeResultPair(TR::TreeTop *tree, TR::Node *result, bool requiresHCRGuard)
         : _tree(tree), _result(result), _requiresHCRGuard(requiresHCRGuard) {}
   };

   TR::VP_BCDSign **_bcdSignConstraints;
   List<TreeNodeResultPair> _callsToBeFoldedToNode;

   struct ValueTypesHelperCallTransform;
   struct ObjectComparisonHelperCallTransform;
   struct ArrayOperationHelperCallTransform;
   struct ArrayElementLoadHelperCallTransform;
   struct ArrayElementStoreHelperCallTransform;

   /**
    * \brief Base class for tracking delayed transformations of value types helper calls
    */
   struct ValueTypesHelperCallTransform
      {
      TR_ALLOC(TR_Memory::ValuePropagation)

      TR::TreeTop *_tree;
      TR::Node *_callNode;
      flags8_t _flags;

      enum // flag bits
         {
         unused1                   = 0x01,
         unused2                   = 0x02,
         InlineVTCompare           = 0x04,
         InsertDebugCounter        = 0x08,
         RequiresBoundCheck        = 0x10,
         RequiresStoreCheck        = 0x20,
         RequiresNullValueCheck    = 0x40,
         IsFlattenedElement        = 0x80, // Indicates whether or not the array elements are flattened in array load or array store.
         };

      ValueTypesHelperCallTransform(TR::TreeTop *tree, TR::Node *callNode, flags8_t flags)
         : _tree(tree), _callNode(callNode), _flags(flags) {}

      /**
       * \brief Indicates whether this represents a delayed transformation for a call to
       *        the value types <objectEqualityCompare> or <objectInequalityCompare> helper
       *
       * \return \c true if and only if this represents a delayed transformation for a
       *        call to the <objectEqualityCompare> or <objectInequalityCompare> helper
       */
      virtual bool isObjectComparisonHelperCallTransform()
         {
         return false;
         }

      /**
       * \brief Indicates whether this represents a delayed transformation for a call to the value
       *        types <jitLoadFlattenableArrayElement> or <jitStoreFlattenableArrayElement> helper
       *
       * \return \c true if and only if this represents a delayed transformation for a
       *        call to the <jitLoadFlattenableArrayElement> or <jitStoreFlattenableArrayElement>
       *        helper
       */
      virtual bool isArrayOperationHelperCallTransform()
         {
         return false;
         }

      /**
       * \brief Indicates whether this represents a delayed transformation for a
       *        call to the value types <jitLoadFlattenableArrayElement> helper
       *
       * \return \c true if and only if this represents a delayed transformation for a
       *        call to the <jitLoadFlattenableArrayElement>
       */
      virtual bool isArrayElementLoadHelperCallTransform()
         {
         return false;
         }

      /**
       * \brief Indicates whether this represents a delayed transformation for
       * a call to the value types <jitStoreFlattenableArrayElement> helper
       *
       * \return \c true if and only if this represents a delayed transformation for a
       *        call to the <jitStoreFlattenableArrayElement>
       */
      virtual bool isArrayElementStoreHelperCallTransform()
         {
         return false;
         }

      /**
       * \brief Casts this object to a pointer to \ref ObjectComparisonHelperCallTransform,
       * if possible; otherwise, reports a fatal assertion failure.
       */
      ObjectComparisonHelperCallTransform *castToObjectComparisonHelperCallTransform()
         {
         TR_ASSERT_FATAL(isObjectComparisonHelperCallTransform(), "ValueTypesHelperCallTransform is not an ObjectComparisonHelperCallTransform\n");
         return static_cast<ObjectComparisonHelperCallTransform *>(this);
         }

      /**
       * \brief Casts this object to a pointer to \ref ArrayOperationHelperCallTransform,
       * if possible; otherwise, reports a fatal assertion failure.
       */
      ArrayOperationHelperCallTransform *castToArrayOperationHelperCallTransform()
         {
         TR_ASSERT_FATAL(isArrayOperationHelperCallTransform(), "ValueTypesHelperCallTransform is not an ArrayOperationHelperCallTransform\n");
         return static_cast<ArrayOperationHelperCallTransform *>(this);
         }

      /**
       * \brief Casts this object to a pointer to \ref ArrayElementLoadHelperCallTransform,
       * if possible; otherwise, reports a fatal assertion failure.
       */
      ArrayElementLoadHelperCallTransform *castToArrayElementLoadHelperCallTransform()
         {
         TR_ASSERT_FATAL(isArrayElementLoadHelperCallTransform(), "ValueTypesHelperCallTransform is not an ArrayElementLoadHelperCallTransform\n");
         return static_cast<ArrayElementLoadHelperCallTransform *>(this);
         }

      /**
       * \brief Casts this object to a pointer to \ref ArrayElementStoreHelperCallTransform,
       * if possible; otherwise, reports a fatal assertion failure.
       */
      ArrayElementStoreHelperCallTransform *castToArrayElementStoreHelperCallTransform()
         {
         TR_ASSERT_FATAL(isArrayElementStoreHelperCallTransform(), "ValueTypesHelperCallTransform is not an ArrayElementStoreHelperCallTransform\n");
         return static_cast<ArrayElementStoreHelperCallTransform *>(this);
         }
      };

   /**
    * \brief Base class for tracking delayed transformations of value types
    * helper call <objectEqualityCompare> and <objectInequalityCompare>
    */
   struct ObjectComparisonHelperCallTransform : ValueTypesHelperCallTransform
      {
      TR_OpaqueClassBlock *_containingClass;

      ObjectComparisonHelperCallTransform(TR::TreeTop *tree, TR::Node *callNode, flags8_t flags, TR_OpaqueClassBlock * containingClass = NULL)
         : ValueTypesHelperCallTransform(/* ComparisonHelper, */ tree, callNode, flags), _containingClass(containingClass) {}

      /**
       * \brief Indicates whether this represents a delayed transformation for a call to
       *        the value types <objectEqualityCompare> or <objectInequalityCompare> helper
       *
       * \return \c true if and only if this represents a delayed transformation for a
       *        call to the <objectEqualityCompare> or <objectInequalityCompare> helper
       */
      virtual bool isObjectComparisonHelperCallTransform()
         {
         return true;
         }
      };

   /**
    * \brief Class for tracking delayed transformations of value types
    * helper calls <jitLoadFlattenableArrayElement> and
    * <jitStoreFlattenableArrayElement>
    */
   struct ArrayOperationHelperCallTransform : ValueTypesHelperCallTransform
      {
      TR_OpaqueClassBlock *_arrayClass;
      int32_t _arrayLength;

      ArrayOperationHelperCallTransform(TR::TreeTop *tree, TR::Node *callNode, flags8_t flags, int32_t arrayLength,
                                        TR_OpaqueClassBlock *arrayClass = NULL)
         : ValueTypesHelperCallTransform(tree, callNode, flags), _arrayClass(arrayClass), _arrayLength(arrayLength) {}


      /**
       * \brief Indicates whether this represents a delayed transformation for a call to the value
       *        types <jitLoadFlattenableArrayElement> or <jitStoreFlattenableArrayElement> helper
       *
       * \return \c true if and only if this represents a delayed transformation for a
       *        call to the <jitLoadFlattenableArrayElement> or <jitStoreFlattenableArrayElement>
       *        helper
       */
      virtual bool isArrayOperationHelperCallTransform()
         {
         return true;
         }
      };

   /**
    * \brief Class for tracking delayed transformations of value types
    * helper calls <jitLoadFlattenableArrayElement>
    */
   struct ArrayElementLoadHelperCallTransform : ArrayOperationHelperCallTransform
      {
      ArrayElementLoadHelperCallTransform(TR::TreeTop *tree, TR::Node *callNode, flags8_t flags, int32_t arrayLength,
                                          TR_OpaqueClassBlock *arrayClass = NULL)
         : ArrayOperationHelperCallTransform(tree, callNode, flags, arrayLength, arrayClass) {}

      /**
       * \brief Indicates whether this represents a delayed transformation for a
       *        call to the value types <jitLoadFlattenableArrayElement> helper
       *
       * \return \c true if and only if this represents a delayed transformation for a
       *        call to the <jitLoadFlattenableArrayElement>
       */
      virtual bool isArrayElementLoadHelperCallTransform()
         {
         return true;
         }
      };

   /**
    * \brief Class for tracking delayed transformations of value types
    * helper calls <jitStoreFlattenableArrayElement>
    */
   struct ArrayElementStoreHelperCallTransform : ArrayOperationHelperCallTransform
      {
      TR_OpaqueClassBlock *_storeClassForArrayStoreCHK;
      TR_OpaqueClassBlock *_componentClassForArrayStoreCHK;

      ArrayElementStoreHelperCallTransform(TR::TreeTop *tree, TR::Node *callNode, flags8_t flags, int32_t arrayLength,
                                           TR_OpaqueClassBlock *arrayClass = NULL, TR_OpaqueClassBlock *storeClassForCheck = NULL,
                                           TR_OpaqueClassBlock *componentClassForCheck = NULL)
         : ArrayOperationHelperCallTransform(tree, callNode, flags, arrayLength, arrayClass),
                     _storeClassForArrayStoreCHK(storeClassForCheck), _componentClassForArrayStoreCHK(componentClassForCheck) {}


      /**
       * \brief Indicates whether this represents a delayed transformation for
       * a call to the value types <jitStoreFlattenableArrayElement> helper
       *
       * \return \c true if and only if this represents a delayed transformation for a
       *        call to the <jitStoreFlattenableArrayElement>
       */
      virtual bool isArrayElementStoreHelperCallTransform()
         {
         return true;
         }
      };

   List<ValueTypesHelperCallTransform> _valueTypesHelperCallsToBeFolded;
   };


}

#endif
