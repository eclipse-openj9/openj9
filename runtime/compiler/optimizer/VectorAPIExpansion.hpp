/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
#ifndef VECTORAPIEXPANSION_INCL
#define VECTORAPIEXPANSION_INCL

#include <stdint.h>
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"

namespace TR { class Block; }


/** \brief
 *  Expands recognized Vector API calls applied to Vector objects(JEPs 338 and 414) into a sequence of
 *  scalar(scalarization) or vector(vectorization) IL opcodes applied to scalar or vector temps correspondingly.
 *
 *  \details
 *  The transformation is based on a light-weight Escape Analysis based on symbol references:
 *  -# peform a quick pass through the treetops to see if there are any Vector API calls in the method,
 *     return if there are no any to avoid unnecessary analysis
 *  -# use the following data structures:
 *      - \c _aliasTable: alias table, indexed by symbol reference number. It is used for aliasing and class validation
 *      - \c _nodeTable: node table, indexed by global node index. It is used for mapping original nodes to their scalar equivalents
 *      - \c _methodTable: table of method handlers indexed by the recognized method id
 *  -# perform a pass through all nodes while visiting each node once:
 *      - "alias"(not in the optimizer sense) encountered symbol references if they are assigned to each other
 *      - "alias" calls with their arguments
 *      - invalidate symbol references that have address taken, dereferenced, or returned
 *      - invalidate symbol references that are assigned or passed \c aconst
 *      - record vector element type and number of lanes(elements) for the recognized Vector API calls.
 *        Note: the element type and the number of lanes should be folded by previous optimizations
 *  -# find transitive closures of all aliased symbol references and place them into distinct classes
 *  -# perform validation of each class. A whole class is invalided if it contains:
 *      - a shadow or parameter symbol reference
 *      - a method symbol reference that is not a recognized Vector API call
 *      - a reference that has been invalidated
 *      - symbol references with different recorded types or number of lanes
 *  -# transform valid classes based on their element type and the number of lanes
 *      - currently only scalarization is supported
 *      - each Vector symbol reference is replaced with several scalar temps, one for each vector lane
 */
class TR_VectorAPIExpansion : public TR::Optimization
   {
   public:
   TR_VectorAPIExpansion(TR::OptimizationManager *manager);

   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_VectorAPIExpansion(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private:

   typedef int32_t vec_sz_t;

   static int32_t const _firstMethod = TR::FirstVectorMethod;
   static int32_t const _lastMethod = TR::LastVectorMethod;

   static int32_t const _numMethods = _lastMethod - _firstMethod + 1;

   // max number of arguments in the recognized Vector API methods
   static int32_t const _maxNumberArguments = 10;

   // max number of operands in a vector operation (e.g. unary, binary, ternary, etc)
   static int32_t const _maxNumberOperands = 5;

   public:

   // TODO: get up to date values from VectorSupport class

   // Unary
   static int32_t const VECTOR_OP_ABS  = 0;
   static int32_t const VECTOR_OP_NEG  = 1;
   static int32_t const VECTOR_OP_SQRT = 2;

   // Binary
   static int32_t const VECTOR_OP_ADD  = 4;
   static int32_t const VECTOR_OP_SUB  = 5;
   static int32_t const VECTOR_OP_MUL  = 6;
   static int32_t const VECTOR_OP_DIV  = 7;
   static int32_t const VECTOR_OP_MIN  = 8;
   static int32_t const VECTOR_OP_MAX  = 9;

   static int32_t const VECTOR_OP_AND  = 10;
   static int32_t const VECTOR_OP_OR   = 11;
   static int32_t const VECTOR_OP_XOR  = 12;

   // Ternary
   static int32_t const VECTOR_OP_FMA  = 13;

   // Broadcast int
   static int32_t const VECTOR_OP_LSHIFT  = 14;
   static int32_t const VECTOR_OP_RSHIFT  = 15;
   static int32_t const VECTOR_OP_URSHIFT = 16;

   static int32_t const VECTOR_OP_CAST        = 17;
   static int32_t const VECTOR_OP_UCAST       = 18;
   static int32_t const VECTOR_OP_REINTERPRET = 19;

   // Mask manipulation operations
   static int32_t const VECTOR_OP_MASK_TRUECOUNT = 20;
   static int32_t const VECTOR_OP_MASK_FIRSTTRUE = 21;
   static int32_t const VECTOR_OP_MASK_LASTTRUE  = 22;
   static int32_t const VECTOR_OP_MASK_TOLONG    = 23;

   // Compare
   static int32_t const BT_eq = 0;
   static int32_t const BT_ne = 4;
   static int32_t const BT_le = 5;
   static int32_t const BT_ge = 7;
   static int32_t const BT_lt = 3;
   static int32_t const BT_gt = 1;
   static int32_t const BT_overflow = 2;
   static int32_t const BT_no_overflow = 6;
   static int32_t const BT_unsigned_compare = 0x10;
   static int32_t const BT_ule = BT_le | BT_unsigned_compare;
   static int32_t const BT_uge = BT_ge | BT_unsigned_compare;
   static int32_t const BT_ult = BT_lt | BT_unsigned_compare;
   static int32_t const BT_ugt = BT_gt | BT_unsigned_compare;

   // Various broadcasting modes.
   static int32_t const MODE_BROADCAST = 0;
   static int32_t const MODE_BITS_COERCED_LONG_TO_MASK = 1;

   // Position of the parameters in the intrinsics.
   static int32_t const BROADCAST_TYPE_CHILD = 4;

  /** \brief
   *  Is passed to methods handlers during analysis and transforamtion phases
   *
   */
   enum handlerMode
      {
      checkScalarization,
      checkVectorization,
      doScalarization,
      doVectorization
      };

  /** \brief
   *  Used to specify whether a specific parameter of a Vector API method specifies vector object,
   *  vector species, element type, or number of lanes
   *
   */
   enum vapiObjType
      {
      Unknown = 0,
      Vector,
      Species,
      ElementType,
      NumLanes,
      Mask,
      Scalar,
      Shuffle,
      Invalid
      };


  /** \brief
   *  Used to specify Vector API opcode category
   *
   */
   enum vapiOpCodeType
      {
      Compare,
      MaskReduction,
      Reduction,
      Test,
      Blend,
      Convert,
      Other
      };


  /** \brief
   *  Entry of the method handlers table
   */
   struct methodTableEntry
      {
      TR::Node * (* _methodHandler)(TR_VectorAPIExpansion *, TR::TreeTop *, TR::Node *, TR::DataType, TR::VectorLength, int32_t, handlerMode);
      vapiObjType  _returnType;
      int32_t      _elementTypeIndex;
      int32_t      _numLanesIndex;
      int32_t      _firstOperandIndex;
      int32_t      _numOperands;
      int32_t      _maskIndex;
      vapiObjType  _argumentTypes[_maxNumberArguments];
      };

   static const vec_sz_t vec_len_unknown = -1;
   static const vec_sz_t vec_len_default = 0;

  /** \brief
   *     Element of the alias table.
   *  -# After a call to \c buildVectorAliases():
   *        - for each symbol reference,  \c _aliases points to all symbol references it is aliased with.
   *        - \c _aliases are symmetrical
   *        - if \c _aliases is \c null it's equivalent to having \c _aliases consisting of one element
   *          which is the symbol reference itself
   *        - \c _classId can be either 0(unknown) or -1(invalidated)
   *  -# after a call to \c buildAliasClasses():
   *        - Symbol references for which \c classId is equal to the symbol reference number are considered classes.
   *        - Their \c _aliases contain all symrefs belonging to that class
   *        - Each symbol reference belongs to exactly one class. Its \c _classId points to that class, unless it's -1
   *  -# after a call to \c validateVectorAliasClasses():
   *         - \c _classId of a class becomes -1 if it was invalidated, otherwise it's a valid class that will be transformed
   *
   */
   class vectorAliasTableElement
      {
      public:
      TR_ALLOC(TR_Memory::Inliner);  // TODO: add new type

      vectorAliasTableElement() : _symRef(NULL), _vecSymRef(NULL),
                                  _vecLen(vec_len_default), _elementType(TR::NoType), _aliases(NULL), _classId(0),
                                  _cantVectorize(false), _cantScalarize(false), _objectType(Unknown),
                                  _tempAliases(NULL), _tempClassId(0) {}

      TR::SymbolReference *_symRef;
      union
         {
         TR::SymbolReference *_vecSymRef;
         TR_Array<TR::SymbolReference*> *_scalarSymRefs;
         };

     /** \brief
      *   Total vector length in bits
      */
      vec_sz_t             _vecLen;
      TR::DataType         _elementType;
      TR_BitVector        *_aliases;
      int32_t              _classId;
      bool                 _cantVectorize;
      bool                 _cantScalarize;
      vapiObjType          _objectType;

      TR_BitVector        *_tempAliases;
      int32_t              _tempClassId;
      };


  /** \brief
   *     Element of the node table. Each element contains an array of scalar nodes
   *     corresponding to each vector lane
   */
   class nodeTableElement
      {
      public:
      TR_ALLOC(TR_Memory::Inliner);  // TODO: add new type

      nodeTableElement() : _scalarNodes(NULL) {}

      TR_Array<TR::Node *> *_scalarNodes;
      };

   TR_Array<vectorAliasTableElement> _aliasTable;
   TR_Array<nodeTableElement> _nodeTable;


   static methodTableEntry methodTable[];

   bool _trace;
   TR_BitVector _visitedNodes;
   TR_BitVector _seenClasses;

  /** \brief
   *     Checks if vector is supported on current platform
   *
   *  \param comp
   *     Compilation
   *
   *  \param vectorLength
   *     Vector length in bits
   *
   *  \return
   *     \c corresponding TR::VectorLength enum if plaform supports \c vectorLength
   *     \c TR::NoVectorLength otherwise
   */
   static TR::VectorLength supportedOnPlatform(TR::Compilation *comp, vec_sz_t vectorLength)
         {
         // General check for supported infrastructure
         if (!comp->target().cpu.isPower() &&
               !(comp->target().cpu.isZ() && comp->cg()->getSupportsVectorRegisters()) &&
               !comp->target().cpu.isARM64())
            return TR::NoVectorLength;

         if (vectorLength != 128)
            return TR::NoVectorLength;

         TR::VectorLength length = OMR::DataType::bitsToVectorLength(vectorLength);

         TR_ASSERT_FATAL(length > TR::NoVectorLength && length <= TR::NumVectorLengths,
                         "VectorAPIExpansion requested invalid vector length %d\n", length);

         return length;
         }

   /** \brief
    *     Checks if the method being compiled contains any recognized Vector API methods
    *
    *  \param comp
    *     Compilation
    *
    *  \return
    *     \c true if it finds any methods,
    *     \c false otherwise
    */
   static bool findVectorMethods(TR::Compilation *comp);

  /** \brief
   *     The method that does the final transformation
   */
   int32_t expandVectorAPI();

  /** \brief
   *     Checks if methodSymbol is a recognized Vector API method
   *
   *  \param methodSymbol
   *     Method symbol
   *
   *  \return
   *     \c true if \c methodSymbol is a recognized Vector API method(high level or intrinsic),
   *     \c false otherwise
   */
   static bool isVectorAPIMethod(TR::MethodSymbol * methodSymbol);


  /** \brief
   *     Returns return type (\c vapiObjType) for the method
   *
   *  \param methodSymbol
   *     Method symbol
   *
   *  \return
   *     \c vapiObjType
   */
   vapiObjType getReturnType(TR::MethodSymbol * methodSymbol);


  /** \brief
   *     Returns index of a child node that contains element type
   *
   *  \param methodSymbol
   *     Method symbol
   *
   *  \return
   *     Index of a child node that contains element type
   */
   int32_t getElementTypeIndex(TR::MethodSymbol *methodSymbol);

  /** \brief
   *     Returns index of a child node that contains number of lanes
   *
   *  \param methodSymbol
   *     Method symbol
   *
   *  \return
   *     Index of a child node that contains number of lanes
   */
   int32_t getNumLanesIndex(TR::MethodSymbol *methodSymbol);

  /** \brief
   *     Returns index of a child node that contains first operand
   *
   *  \param methodSymbol
   *     Method symbol
   *
   *  \return
   *     Index of a child node that contains first operand
   */
   int32_t getFirstOperandIndex(TR::MethodSymbol *methodSymbol);

  /** \brief
   *     Returns number of operands
   *
   *  \param methodSymbol
   *     Method symbol
   *
   *  \return
   *     Number of operands
   */
   int32_t getNumOperands(TR::MethodSymbol *methodSymbol);

  /** \brief
   *     Returns index of a child node that contains mask
   *
   *  \param methodSymbol
   *     Method symbol
   *
   *  \return
   *     Index of a child node that contains mask
   */
   int32_t getMaskIndex(TR::MethodSymbol *methodSymbol);

  /** \brief
   *     Determines element type and number of lanes of a node
   *
   *  \param node
   *     Call node
   *
   *  \param elementType
   *     Element type
   *
   *  \param numLanes
   *     Number of lanes
   */
   void getElementTypeAndNumLanes(TR::Node *node, TR::DataType &elementType, int32_t &numLanes);

  /** \brief
   *     Aliases symbol references with each other as described above
   *
   */
   void buildVectorAliases();

  /** \brief
   *     Used by \c buildVectorAliases() to visit nodes recursively
   */
   void visitNodeToBuildVectorAliases(TR::Node *node);

  /** \brief
   *     Finds transitive closures of the alias sets built by \c buildVectorAliases()
   */
   void buildAliasClasses();

  /** \brief
   *     Recursive method used by \c buildAliasClasses()
   *
   *   \param classId
   *     Class to which to add all transitive aliases of \c id
   *
   *   \param id
   *     Symbol reference for which all transitive aliases need to be found
   *
   *   \param aliasesField
   *     Pointer to the struct member that contains aliases
   *
   *   \param classField
   *     Pointer to the struct member that contains class
   */
   void findAllAliases(int32_t classId, int32_t id, TR_BitVector * vectorAliasTableElement::* aliasesField, int32_t vectorAliasTableElement::* classField);

  /** \brief
   *     Validates classes found by \c buildAliasClasses()
   *
   *   \param aliasesField
   *     Pointer to the struct member that contains aliases
   *
   *   \param classField
   *     Pointer to the struct member that contains class
   */
   void validateVectorAliasClasses(TR_BitVector * vectorAliasTableElement::* aliasesField, int32_t vectorAliasTableElement::* classField);

  /** \brief
   *     Used by \c validateVectorAliasClasses() to check individual symbol reference
   *
   *  \param classId
   *     Class id
   *
   *  \param i
   *     Reference id
   *
   *  \param classLength
   *     Number of lanes in theclass
   *
   *  \param classType
   *     Element type of the class
   *
   *  \param classField
   *     Pointer to the struct member that contains class
   *
   */
   bool validateSymRef(int32_t classId, int32_t i, vec_sz_t &classLength, TR::DataType &classType, int32_t vectorAliasTableElement::* classField);

  /** \brief
   *     Sets \c _classId of a symbol reference to -1 to indicate it's invalid
   *
   *   \param symRef
   *     Symbol reference
   *
   */
   void invalidateSymRef(TR::SymbolReference *symRef);

  /** \brief
   *     Adds symbol references of two nodes to each other alias sets
   *
   *  \param node1
   *     First node
   *
   *  \param node2
   *     Second node
   *
   *  \param aliasTemps
   *     true if aliasing is caused by storing one temp into another
   */
   void alias(TR::Node *node1, TR::Node *node2, bool aliasTemps = false);

  /** \brief
   *     Finds vector length from SPECIES node if it's a known object
   *
   *  \param vectorSpeciesNode
   *     Node that loads SPECIES object
   */
   vec_sz_t getVectorSizeFromVectorSpecies(TR::Node *vectorSpeciesNode);


  /** \brief
   *     Returns J9Class for a known object being loaded by node
   *
   *  \param comp
   *     Compilation
   *
   *  \param classNode
   *     Node that loads \c java/lang/Class
   */
   static J9Class *getJ9ClassFromClassNode(TR::Compilation *comp, TR::Node *classNode);


  /** \brief
   *     Returns corresponding \c vapiObjType for a known object being loaded by node
   *
   *  \param comp
   *     Compilation
   *
   *  \param classNode
   *     Node that loads \c java/lang/Class
   */
   static vapiObjType getObjectTypeFromClassNode(TR::Compilation *comp, TR::Node *classNode);

  /** \brief
   *     Maps object of type \c java/lang/Class (e.g., \c java/lang/Float.TYPE)
   *     into corresponding \c TR::DataType
   *
   *  \param comp
   *     Compilation
   *
   *  \param classNode
   *     Node that loads SPECIES object
   */
   static TR::DataType getDataTypeFromClassNode(TR::Compilation *comp, TR::Node *classNode);

  /** \brief
   *     Anchors all node's children before transforming it into a new node
   *
   *  \param opt
   *     this optimization object
   *
   *  \param treeTop
   *     treetop of the current node
   *
   *  \param node
   *     node
   */
   static void anchorOldChildren(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node);


  /** \brief
   *     Generates address node based on the array pointer, array index  and element size
   *
   *  \param array
   *     Node pointing to the base of the array
   *
   *  \param arrayIndex
   *     Array index
   *
   *  \param elementSize
   *     Element size
   *
   *  \return
   *     Address node
   */
   static TR::Node *generateAddressNode(TR::Node *array, TR::Node *arrayIndex, int32_t elementSize);


  /** \brief
   *     Maps Vector API opcode enum into scalar or vector TR::ILOpCodes
   *
   *  \param comp
   *     Compilation
   *
   *  \param vectorOpCode
   *     Vector API opcode enum
   *
   *  \param elementType
   *     Element type
   *
   *  \param vectorLength
   *     return scalar opcode if vectorLength == 0 and vector opcode otherwise
   *
   *  \param opCodeType
   *     opcode type
   *
   *  \param withMask
   *     true if mask is present, false otherwise
   *
   *  \param resultElementType
   *     Result element type
   *
   *  \param resultVectorLength
   *     Result vector length
   *
   *  \return
   *     scalar TR::IL opcode if scalar is true, otherwise vector opcode
   */
   static TR::ILOpCodes ILOpcodeFromVectorAPIOpcode(TR::Compilation *comp, int32_t vectorOpCode, TR::DataType elementType,
                                                    TR::VectorLength vectorLength, vapiOpCodeType opCodeType, bool withMask,
                                                    TR::DataType resultElementType = TR::NoType,
                                                    TR::VectorLength resultVectorLength = TR::NoVectorLength);

  /** \brief
   *    For the node's symbol reference, creates and records(if it does not exist yet)
   *    corresponding vector temporary symbol and symbol reference. Changes node's symbol reference and opcode
   *    to corresponding vector(or mask) symbol reference and opcode
   *
   *  \param opt
   *    This optimization object
   *
   *  \param node
   *    The node
   *
   *  \param opCodeType
   *    Opcode type
   *
   */
   static void vectorizeLoadOrStore(TR_VectorAPIExpansion *opt, TR::Node *node, TR::DataType opCodeType);

  /** \brief
   *    For the node's symbol reference, creates and records, if it does not exist yet,
   *    corresponding \c numLanes scalar temporary symbols and symbol references.
   *    Changes node's symbol reference and opcode to corresponding vector symbol reference and opcode
   *
   *  \param opt
   *    This optimization
   *
   *  \param node
   *    The node
   *
   *  \param elementType
   *    Element type used to create new temporary
   *
   *  \param numLanes
   *    Element type used to create new temporary
   */
   static void scalarizeLoadOrStore(TR_VectorAPIExpansion *opt, TR::Node *node, TR::DataType elementType, int32_t numLanes);


  /** \brief
   *    Adds scalar node corresponding to lane \c i to the array of scalar nodes corrsponding to
   *    the original node
   *
   *   \param opt
   *      This optimization object
   *
   *   \param node
   *      Orignal node for which scalar node is created
   *
   *   \param numLanes
   *      Number of lanes used to create an array of scalar nodes
   *
   *   \param i
   *      Lane number for which scalar node is created
   *
   *   \param scalarNode
   *      Scalar node
   *
   */
   static void addScalarNode(TR_VectorAPIExpansion *opt, TR::Node *node, int32_t numLanes, int32_t i, TR::Node *scalarNode);


  /** \brief
   *    Gets scalar node corresponding to lane \c i from  the array of scalar nodes corrsponding to
   *    the original node
   *
   *   \param opt
   *      This optimization object
   *
   *   \param node
   *      Orignal node
   *
   *   \param i
   *      Lane number to which scalar node corresponds to
   *
   *   \return
   *      Scalar node
   */
   static TR::Node *getScalarNode(TR_VectorAPIExpansion *opt, TR::Node *node, int32_t i);

  /** \brief
   *    Scalarizes or vectorizes \c aload node. In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *       Number of elements
   *
   *   \param mode
   *      \c handlerMode
   *
   *   \return
   *      Transformed node
   *
   */
   static void aloadHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);

  /** \brief
   *    Scalarizes or vectorizes \c astore node. In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *       Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   */
   static void astoreHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);

  /** \brief
   *    Handler used for unsupported methods
   *
   *   \return
   *      NULL
   */
   static TR::Node *unsupportedHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);

  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c VectorSupport.load() intrinsic.
   *    In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *       Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \return
   *      Transformed node
   *
   */
   static TR::Node *loadIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);


  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c VectorSupport.store() intrinsic.
   *    In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \return
   *      Transformed node
   *
   */
   static TR::Node *storeIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);

  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c VectorSupport.unaryOp() intrinsic.
   *    In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \return
   *      Transformed node
   *
   */
   static TR::Node *unaryIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);

  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c VectorSupport.binaryOp() intrinsic.
   *    In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \return
   *      Transformed node
   *
   */
   static TR::Node *binaryIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);

  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c VectorSupport.maskReductionCoerced() intrinsic.
   *    In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \return
   *      Transformed node
   *
   */
   static TR::Node *maskReductionCoercedIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);

  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c VectorSupport.reductionCoerced() intrinsic.
   *    In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \return
   *      Transformed node
   *
   */
   static TR::Node *reductionCoercedIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);

  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c VectorSupport.ternaryOp() intrinsic.
   *    In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \return
   *      Transformed node
   *
   */
   static TR::Node *ternaryIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);


  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c VectorSupport.test() intrinsic.
   *    In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \return
   *      Transformed node
   *
   */
   static TR::Node *testIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);

  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c VectorSupport.unaryOp(),binaryOp(), etc. intrinsic.
   *    In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \param numChildren
   *      Number of operands
   *
   *   \param opCodeType
   *      opcode type
   *
   *   \return
   *      Transformed node
   */
   static TR::Node *naryIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode, int32_t numChidren, vapiOpCodeType opCodeType);

  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c VectorSupport.blend() intrinsic.
   *    In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \return
   *      Transformed node
   */
   static TR::Node *blendIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);

  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c VectorSupport.fromBitsCoerced() intrinsic.
   *    In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \return
   *      Transformed node
   */
   static TR::Node *fromBitsCoercedIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);

  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c VectorSupport.compare() intrinsic.
   *    In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \return
   *      Transformed node
   */
   static TR::Node *compareIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);


  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c VectorSupport.convert() intrinsic.
   *    In both cases, the node is modified in place.
   *    In the case of scalarization, extra nodes are created(number of lanes minus one)
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \return
   *      Transformed node
   */
   static TR::Node *convertIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode);


  /** \brief
   *    Helper method to transform a load from array node
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \param array
   *      Array node
   *
   *   \param arrayIndex
   *      array index node
   *
   *   \param objType
   *      Vector API object type (Vector, Mask, Shuffle, etc.)
   *
   *   \return
   *      Transformed node
   *
   */
   static TR::Node *transformLoadFromArray(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode, TR::Node *array, TR::Node *arrayIndex, vapiObjType objType);

  /** \brief
   *    Helper method to transform a store to array node
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \param valueToWrite
   *      Node containing the value to be written
   *
   *   \param array
   *      Array node
   *
   *   \param arrayIndex
   *      array index node
   *
   *   \param objType
   *      Vector API object type (Vector, Mask, Shuffle, etc.)
   *
   *   \return
   *      Transformed node
   *
   */
   static TR::Node *transformStoreToArray(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode, TR::Node *valueToWrite, TR::Node *array, TR::Node *arrayIndex, vapiObjType objType);


  /** \brief
   *    Helper method to transform an nary operation node
   *
   *   \param opt
   *      This optimization object
   *
   *   \param treeTop
   *      Tree top of the \c node
   *
   *   \param node
   *      Node to transform
   *
   *   \param elementType
   *      Element type
   *
   *   \param vectorLength
   *      Vector length
   *
   *   \param numLanes
   *      Number of elements
   *
   *   \param mode
   *      Handler mode
   *
   *   \param scalarOpCode
   *      Scalar Opcode
   *
   *   \param vectorOpCode
   *      Vector Opcode
   *
   *   \param firstOperand
   *      Child index of the first operand
   *
   *   \param numOperands
   *      Number of operands
   *
   *   \param opCodeType
   *      opcode type
   *
   *   \return
   *      Transformed node
   */
   static TR::Node *transformNary(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, TR::VectorLength vectorLength, int32_t numLanes, handlerMode mode, TR::ILOpCodes scalarOpCode, TR::ILOpCodes vectorOpCode, int32_t firstOperand, int32_t numOperands, vapiOpCodeType opCodeType);
   };
#endif /* VECTORAPIEXPANSION_INCL */
