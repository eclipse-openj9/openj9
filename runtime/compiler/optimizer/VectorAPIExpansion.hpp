/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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
#ifndef VECTORAPIEXPANSION_INCL
#define VECTORAPIEXPANSION_INCL

#include <stdint.h>
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "il/SymbolReference.hpp"

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
   TR_VectorAPIExpansion(TR::OptimizationManager *manager)
      : TR::Optimization(manager), _trace(false), _aliasTable(trMemory()), _nodeTable(trMemory())
      {}
   
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_VectorAPIExpansion(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private:
   
   typedef int vec_sz_t;

   static int const _firstMethod = TR::First_vector_api_method + 1;
   static int const _lastMethod = TR::Last_vector_api_method - 1;
   
   static int const _numMethods = _lastMethod - _firstMethod + 1;
   static int const _numArguments = 15;

   // TODO: get up to date values from VectorSupport class
   
   // Unary
   static int const VECTOR_OP_ABS  = 0;
   static int const VECTOR_OP_NEG  = 1;
   static int const VECTOR_OP_SQRT = 2;

   // Binary
   static int const VECTOR_OP_ADD  = 4;
   static int const VECTOR_OP_SUB  = 5;
   static int const VECTOR_OP_MUL  = 6;
   static int const VECTOR_OP_DIV  = 7;
   static int const VECTOR_OP_MIN  = 8;
   static int const VECTOR_OP_MAX  = 9;

   static int const VECTOR_OP_AND  = 10;
   static int const VECTOR_OP_OR   = 11;
   static int const VECTOR_OP_XOR  = 12;

   // Ternary
   static int const VECTOR_OP_FMA  = 13;

   // Broadcast int
   static int const VECTOR_OP_LSHIFT  = 14;
   static int const VECTOR_OP_RSHIFT  = 15;
   static int const VECTOR_OP_URSHIFT = 16;

   static int const VECTOR_OP_CAST        = 17;
   static int const VECTOR_OP_REINTERPRET = 18;

   public:

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
   enum vapiArgType
      {
      Unknown = 0,
      Vector,
      Species,
      elementType,
      numLanes
      };


  /** \brief
   *  Entry of the method handlers table
   */
   struct methodTableEntry
      {
      TR::Node * (* _methodHandler)(TR_VectorAPIExpansion *, TR::TreeTop *, TR::Node *, TR::DataType, vec_sz_t, handlerMode);
      TR::DataType _elementType;
      vapiArgType  _returnType;
      vapiArgType  _argumentTypes[10];
      }; 

   static const vec_sz_t vec_len_unknown = -1;
   static const vec_sz_t vec_len_default = 0;

   static const handlerMode defaultCheckMode = checkScalarization;
   static const handlerMode defaultDoMode = doScalarization;


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
                                  _vecLen(vec_len_default), _elementType(TR::NoType), _aliases(NULL), _classId(0) {}

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

   
   static methodTableEntry methodTable[_numMethods];

   bool _trace;
   TR_BitVector _visitedNodes;
   TR_BitVector _seenClasses;

   /** \brief
    *     Checks if the method being compiled contains any recognized Vector API methods
    *     
    *  \return
    *     \c true if it finds any methods,
    *     \c false otherwise
    */
   bool findVectorMethods(); 
 
  /** \brief
   *     The method that does the final transformation
   *     
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
   bool isVectorAPIMethod(TR::MethodSymbol * methodSymbol);

  /** \brief
   *     Checks if methodSymbol returns Vector object
   *
   *  \param methodSymbol
   *     Method symbol
   *
   *  \return 
   *     \c true if \c methodSymbol returns Vector object,
   *     \c false otherwise
   */
   bool returnsVector(TR::MethodSymbol * methodSymbol);

  /** \brief
   *     Checks if method's argument is one the \c vapiArgType types
   *
   *  \param methodSymbol
   *     Method symbol
   *
   *  \param i
   *     argument's number
   *
   *  \param type
   *     \c vapiArgType
   *
   *  \return 
   *     \c true if the argument is the same as \c type,
   *     \c false otherwise
   */
   bool isArgType(TR::MethodSymbol *methodSymbol, int i, vapiArgType type);

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
   */   
   void findAllAliases(int32_t classId, int32_t id);

  /** \brief
   *     Validates classes found by \c buildAliasClasses()
   */   
   void validateVectorAliasClasses();

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
   */   
   bool validateSymRef(int32_t classId, int32_t i, vec_sz_t &classLength, TR::DataType &classType);

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
   */   
   void alias(TR::Node *node1, TR::Node *node2);

  /** \brief
   *     Finds vector length from SPECIES node if it's a known object
   *
   *  \param vectorSpeciesNode
   *     Node that loads SPECIES object
   */
   vec_sz_t getVectorSizeFromVectorSpecies(TR::Node *vectorSpeciesNode);

  /** \brief
   *     Maps object of type \c java/lang/Class (e.g., \c java/lang/Float.TYPE)
   *     into corresponding \c TR::DataType
   *
   *  \param classNode
   *     Node that loads SPECIES object
   */
   TR::DataType getDataTypeFromClassNode(TR::Node *classNode);

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
   *     Maps Vector API opcode enum into TR::ILOpCodes
   *
   *  \param vectorOpCode
   *     Vector API opcode enum
   *     
   *  \param elementType
   *     Element type
   *     
   *  \return
   *     TR::IL opcode
   */
   static TR::ILOpCodes ILOpcodeFromVectorAPIOpcode(int vectorOpCode, TR::DataType elementType);

  /** \brief
   *    For the node's symbol reference, creates and records(if it does not exist yet) 
   *    corresponding vector temporary symbol and symbol reference. Changes node's symbol reference and opcode
   *    to corresponding vector symbol reference and opcode
   *
   *  \param opt
   *    This optimization object
   *     
   *  \param node
   *    The node
   *
   *  \param type
   *    Element type
   */
   static void vectorizeLoadOrStore(TR_VectorAPIExpansion *opt, TR::Node *node, TR::DataType type);

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
   static void scalarizeLoadOrStore(TR_VectorAPIExpansion *opt, TR::Node *node, TR::DataType elementType, int numLanes);


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
   static void addScalarNode(TR_VectorAPIExpansion *opt, TR::Node *node, int numLanes, int i, TR::Node *scalarNode);


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
   */
   static TR::Node *getScalarNode(TR_VectorAPIExpansion *opt, TR::Node *node, int i);

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
   *      Full vector length in bits (e.g. 128 for Float128Vector)
   *
   *   \param mode
   *      \c handlerMode
   *
   *   \return
   *      Transformed node
   *
   */
   static void aloadHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode);

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
   *      Full vector length in bits (e.g. 128 for Float128Vector)
   *
   *   \param mode
   *      Handler mode
   *     
   */
   static void astoreHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode);

  /** \brief
   *    Handler used for unsupported methods
   *    
   *   \return
   *      NULL
   */
   static TR::Node *unsupportedHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode);

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
   *      Full vector length in bits (e.g. 128 for Float128Vector)
   *
   *   \param mode
   *      Handler mode
   *     
   */
   static TR::Node *loadIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode);


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
   *      Full vector length in bits (e.g. 128 for Float128Vector)
   *
   *   \param mode
   *      Handler mode
   *     
   */
   static TR::Node *storeIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode);

      
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
   *      Full vector length in bits (e.g. 128 for Float128Vector)
   *
   *   \param mode
   *      Handler mode
   *     
   */
   static TR::Node *binaryIntrinsicHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode);


  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c intoArray() Vector API method.
   *    In both cases, the node is modified in place. 
   *    In the case of scalarization, extra nodes are created(lanes minus one nodes)
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
   *      Full vector length in bits (e.g. 128 for Float128Vector)
   *
   *   \param mode
   *      Handler mode
   *     
   */
   static TR::Node *intoArrayHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode);


  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c fromArray() Vector API method.
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
   *      Full vector length in bits (e.g. 128 for Float128Vector)
   *
   *   \param mode
   *      Handler mode
   *     
   */
   
   static TR::Node *fromArrayHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode);

  /** \brief
   *    Scalarizes or vectorizes a node that is a call to \c add() Vector API method.
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
   *      Full vector length in bits (e.g. 128 for Float128Vector)
   *
   *   \param mode
   *      Handler mode
   *     
   */
   static TR::Node *addHandler(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode);

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
   *      Full vector length in bits (e.g. 128 for Float128Vector)
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
   */
   static TR::Node *transformLoad(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode, TR::Node *array, TR::Node *arrayIndex);

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
   *      Full vector length in bits (e.g. 128 for Float128Vector)
   *
   *   \param mode
   *      Handler mode
   *
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
   */
   static TR::Node *transformStore(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode, TR::Node *valueToWrite, TR::Node *array, TR::Node *arrayIndex);


  /** \brief
   *    Helper method to transform a binary operation node
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
   *      Full vector length in bits (e.g. 128 for Float128Vector)
   *
   *   \param mode
   *      Handler mode
   *
   *   \param firstChild
   *      Node containing first operand
   *
   *   \param secondChild
   *      Node containing second operand
   *
   *   \param opcode
   *      Opcode
   */
   static TR::Node *transformBinary(TR_VectorAPIExpansion *opt, TR::TreeTop *treeTop, TR::Node *node, TR::DataType elementType, vec_sz_t vectorLength, handlerMode mode, TR::Node *firstChild, TR::Node *secondChild, TR::ILOpCodes opcode);

   };
#endif
