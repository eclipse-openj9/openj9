/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include <algorithm>
#include "codegen/CodeGenerator.hpp"
#include "compile/ResolvedMethod.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "optimizer/PreExistence.hpp"
#include "ras/LogTracer.hpp"
#include "env/VMJ9.h"
#include "optimizer/J9CallGraph.hpp"
#include "optimizer/J9EstimateCodeSize.hpp"
#include "runtime/J9Profiler.hpp"

// Empirically determined value
const float TR_J9EstimateCodeSize::STRING_COMPRESSION_ADJUSTMENT_FACTOR = 0.75f;

#define NUM_PREV_BC 5

/*
DEFINEs are ugly in general, but putting
   if (tracer)
      heuristicTrace(...)
everywhere in this class seems to be a much worse idea.
Unfortunately, C++98 doesn't have a good way to forward varargs
except for using a DEFINE
*/

#define heuristicTraceIfTracerIsNotNull(r, ...) \
if (r) { \
   if ((r)->heuristicLevel()) { (r)->alwaysTraceM(__VA_ARGS__); } \
}
class NeedsPeekingHeuristic
{
   public:

      static const int default_distance = 25;
      static const int NUM_LOADS = 4;

      NeedsPeekingHeuristic(TR_CallTarget* calltarget, TR_J9ByteCodeIterator& bci, TR::ResolvedMethodSymbol* methodSymbol, TR::Compilation* comp, int d = default_distance) :
         _hasArgumentsInfo(false),
         _size(0),
         _bci(bci),
         _distance(d),
         _numOfArgs(0),
         _needsPeeking(false),
         _tracer(0)
         {
         TR_PrexArgInfo* argInfo = calltarget->_ecsPrexArgInfo;
         //no argInfo available for this caller
         if (!argInfo)
            return;

         int i = 0;
         int32_t numParms = methodSymbol->getParameterList().getSize();
         _numOfArgs = numParms;
         ListIterator<TR::ParameterSymbol> parmIt(&methodSymbol->getParameterList());
         for (TR::ParameterSymbol *p = parmIt.getFirst(); p; p = parmIt.getNext(), i++)
            {
            int32_t len;
            const char *sig = p->getTypeSignature(len);
            if (i >= argInfo->getNumArgs() ||            //not enough slots in argInfo
               *sig != 'L' ||                            //primitive arg
               !argInfo->get(i) ||                       //no arg at the i-th slot
               !argInfo->get(i)->getClass()         //no classInfo at the i-th slot
               )
               {
               continue;
               }

            TR_OpaqueClassBlock *clazz = comp->fej9()->getClassFromSignature(sig, len, methodSymbol->getResolvedMethod());
            if (!clazz)
               {
               continue;
               }

            TR_OpaqueClassBlock* argClass = argInfo->get(i)->getClass();
            //findCallSiteTarget and validateAndPropagateArgsFromCalleeSymbol
            //should take care of incompatible receivers
            //this assertion only checks if the receiver is of the right type
            //there's no harm in propagating other incompatible arguments
            //as soon as one of those becomes a receiver this very same assertion
            //should fire
            TR_ASSERT(comp->fej9()->isInstanceOf(argClass, clazz, true, true, true) == TR_yes || i != 0 || !calltarget->_myCallSite->_isIndirectCall, "Incompatible receiver should have been handled by findCallSiteTarget");
            
            // even the arg type propagated from the caller might be not more specific
            // than the type got from callee signature, we should still try to
            // do peeking. if we don't do peeking here, we will lose the chance to propagate
            // the type info to the callsites of this calltarget.
            static const bool keepBogusPeekingCondition = feGetEnv("TR_DisableBogusPeekingCondition") ? false: true;
            if ( !keepBogusPeekingCondition || clazz != argClass ) //if two classes aren't equal it follows that argClass is more specific
                                   //argsClass can either be equal to or a subclass of clazz
                                   //see validateAndPropagateArgsFromCalleeSymbol
               {
               _hasArgumentsInfo = true;
               _argInfo = argInfo;
               }

            /*
            if (comp->fej9()->isInstanceOf (argClass, clazz, true, true, true) == TR_yes)
            {
            if (clazz != argClass)
            _hasArgumentsInfo = true;
            }
            else
            {
            _hasArgumentsInfo = false;
            return;  // _hasArgumentsInfo will be equal to false and no propagation is going to happen
            // because the incoming type information is not compatible
            }
            */
               }
            };

      void setTracer(TR_InlinerTracer *trc)
      {
         _tracer = trc;
         heuristicTraceIfTracerIsNotNull(_tracer, "NeedsPeekingHeuristic is initialized with the following values: _hasArgumentsInfo = %d, NUM_LOADS = %d, _distance =%d, _needsPeeking = %d", _hasArgumentsInfo, NUM_LOADS, _distance, _needsPeeking);
      }

      void checkIfThereIsAParmLoadWithinDistance()
      {
         for (int i = 0; i < _size; i++)
         {
            if (_bci.bcIndex() - _loadIndices[i] <= _distance)
            {
               _needsPeeking = true;
               heuristicTraceIfTracerIsNotNull(_tracer, "there is a parm load at %d which is within %d of a call at %d", _loadIndices[i], _distance, _bci.bcIndex());
            }
         }
      };

      void processByteCode()
         {
            if (!_hasArgumentsInfo)
               return;
            TR_J9ByteCode bc = _bci.current();
            int slotIndex = -1;
            switch (bc)
               {
               case J9BCaload0:
                  slotIndex = 0;
                  break;
               case J9BCaload1:
                  slotIndex = 1;
                  break;
               case J9BCaload2:
                  slotIndex = 2;
                  break;
               case J9BCaload3:
                  slotIndex = 3;
                  break;
               case J9BCaload:
                  slotIndex = _bci.nextByte();
                  TR_ASSERT(slotIndex >= 0 , "a slot shouldn't be negative");
                  break;
               case J9BCaloadw:
                  slotIndex = _bci.next2Bytes();
                  TR_ASSERT(slotIndex >= 0 , "a slot shouldn't be negative");
                  break;
               case J9BCinvokevirtual:
               case J9BCinvokespecial:
               case J9BCinvokestatic:
               case J9BCinvokeinterface:
               case J9BCinvokedynamic:
               case J9BCinvokehandle:
               case J9BCinvokehandlegeneric:
                  checkIfThereIsAParmLoadWithinDistance ();
               default :
                  break;

               }

            if (slotIndex >=0)
               {
               processParameterLoad(slotIndex);
               }

         };


      void processParameterLoad (int slotIndex)
         {
         //This heuristic simply checks if we indeed hit a parameter load (as opposed to an auto)
         //and if we have an argInfo for this slot we would want to propagate
         //Note, _hasArgumentsInfo is checked in processByteCode
         //we should not even reach this code unless we have some PrexInfo
         if (slotIndex < _numOfArgs && _argInfo->get(slotIndex))
            {
            heuristicTraceIfTracerIsNotNull(_tracer,"came across of a load of slot %d at %d", slotIndex, _bci.bcIndex());
            _loadIndices[_size] = _bci.bcIndex();
            _size = (_size + 1) % NUM_LOADS;
            }
         }
      bool doPeeking () { return _needsPeeking; };

   protected:
      int32_t _loadIndices [NUM_LOADS];
      int _size;
      int _numOfArgs;
      int _distance;
      TR_J9ByteCodeIterator& _bci;
      bool _hasArgumentsInfo;
      TR_PrexArgInfo * _argInfo;
      bool _needsPeeking;
      TR_InlinerTracer * _tracer;

};
#undef heuristicTraceIfTracerIsNotNull

class TR_prevArgs
{
   public:
      TR_prevArgs() { for (int32_t i = 0 ; i < NUM_PREV_BC ; i++ ) { _prevBC[i] = J9BCunknown ; } }

      void printIndexes(TR::Compilation *comp)
         {
         for (int32_t i = 0 ; i < NUM_PREV_BC ; i++)
            {
            if(comp->getDebug())
               traceMsg(comp,"_prevBC[%d] = %s\n" ,i,((TR_J9VM*)(comp->fej9()))->getByteCodeName(_prevBC[i]));
            }
         }

      void updateArg(TR_J9ByteCode bc )
      {
      for(int32_t i=NUM_PREV_BC-2 ; i>=0 ; i-- )
         {
         _prevBC[i+1] = _prevBC[i];
         }
      _prevBC[0] = bc;
      }

      bool isArgAtIndexReceiverObject (int32_t index)
         {
         if (  index < NUM_PREV_BC && _prevBC[index] == J9BCaload0)
            {
            return true;
            }
         else
            return false;
         }

      int32_t getNumPrevConstArgs(int32_t numparms)
      {
      int32_t count=0;

      for(int32_t i=0 ; i < NUM_PREV_BC && i < numparms ; i++)
         {
         switch (_prevBC[i])
             {
             case J9BCaconstnull:
             case J9BCiconstm1:
             case J9BCiconst0:
             case J9BCiconst1:
             case J9BCiconst2:
             case J9BCiconst3:
             case J9BCiconst4:
             case J9BCiconst5:
             case J9BClconst0:
             case J9BClconst1:
             case J9BCfconst0:
             case J9BCfconst1:
             case J9BCfconst2:
             case J9BCdconst0:
             case J9BCdconst1:
             case J9BCldc: case J9BCldcw: case J9BCldc2lw: case J9BCldc2dw:
             case J9BCbipush: case J9BCsipush:
                count++;
                break;
             default:
            	 break;
             }
         }
      return count;
      }


   protected:
      TR_J9ByteCode _prevBC[NUM_PREV_BC];
};


static void setupNode(TR::Node *node, uint32_t bcIndex,
                      TR_ResolvedMethod *feMethod, TR::Compilation *comp)
   {
   node->getByteCodeInfo().setDoNotProfile(0);
   node->setByteCodeIndex(bcIndex);
   node->setInlinedSiteIndex(-10);
   node->setMethod(feMethod->getPersistentIdentifier());
   }


// TODO: use a smaller object to represent the block
//#define Block TR::CFGNode
#define Block TR::Block
static Block * getBlock(TR::Compilation *comp, Block * * blocks,
      TR_ResolvedMethod *feMethod, int32_t i, TR::CFG & cfg)
   {
   if (!blocks[i])
      {

      TR::TreeTop *startTree = TR::TreeTop::create(comp, TR::Node::createOnStack(
            NULL, TR::BBStart, 0));
      TR::TreeTop *endTree = TR::TreeTop::create(comp, TR::Node::createOnStack(
            NULL, TR::BBEnd, 0));

      startTree->join(endTree);
      blocks[i] = new (comp->trStackMemory()) Block (startTree, endTree,
            comp->trMemory());

      blocks[i]->setJ9EstimateCodeSizeMethod(feMethod);
      blocks[i]->setBlockBCIndex(i);
      blocks[i]->setNumber(cfg.getNextNodeNumber());

      setupNode(startTree->getNode(), i, feMethod, comp);
      setupNode(endTree->getNode(), i, feMethod, comp);
      cfg.addNode(blocks[i]);
      }

   return blocks[i];
   }

static TR::ILOpCodes convertBytecodeToIL (TR_J9ByteCode bc)
   {
   switch (bc)
      {
      case J9BCifeq: return TR::ificmpeq;
      case J9BCifne: return TR::ificmpne;
      case J9BCiflt: return TR::ificmplt;
      case J9BCifge: return TR::ificmpge;
      case J9BCifgt: return TR::ificmpgt;
      case J9BCifle: return TR::ificmple;
      case J9BCifnull: return TR::ifacmpeq;
      case J9BCifnonnull: return TR::ifacmpne;
      case J9BCificmpeq: return TR::ificmpeq;
      case J9BCificmpne: return TR::ificmpne;
      case J9BCificmplt: return TR::ificmplt;
      case J9BCificmpge: return TR::ificmpge;
      case J9BCificmpgt: return TR::ificmpgt;
      case J9BCificmple: return TR::ificmple;
      case J9BCifacmpeq: return TR::ifacmpeq;
      case J9BCifacmpne: return TR::ifacmpne;
      case J9BCtableswitch:  return TR::table;
      case J9BClookupswitch: return TR::lookup;
      case J9BCgoto:
      case J9BCgotow: return TR::Goto;
      case J9BCgenericReturn: return TR::Return;
      case J9BCathrow: return TR::athrow;
      default:
         TR_ASSERT(0,"Unsupported conversion for now.");
         return TR::BadILOp;
      }
   return TR::BadILOp;
   }

static void setupLastTreeTop(Block *currentBlock, TR_J9ByteCode bc,
                             uint32_t bcIndex, Block *destinationBlock, TR_ResolvedMethod *feMethod,
                             TR::Compilation *comp)
   {
   TR::Node *node = TR::Node::createOnStack(NULL, convertBytecodeToIL(bc), 0);
   TR::TreeTop *tree = TR::TreeTop::create(comp, node);
   setupNode(node, bcIndex, feMethod, comp);
   if (node->getOpCode().isBranch())
      node->setBranchDestination(destinationBlock->getEntry());
   currentBlock->append(tree);
   }


//Partial Inlining
bool
TR_J9EstimateCodeSize::isInExceptionRange(TR_ResolvedMethod * feMethod,
      int32_t bcIndex)
   {
   int32_t numExceptionRanges = feMethod->numberOfExceptionHandlers();

   if (numExceptionRanges == 0)
      return false;

   int32_t start, end, catchtype;

   for (int32_t i = 0; i < numExceptionRanges; i++)
      {
      feMethod->exceptionData(i, &start, &end, &catchtype);
      if (bcIndex > start && bcIndex < end)
         return true;
      }
   return false;
   }

#undef Block

static bool cameFromArchetypeSpecimen(TR_ResolvedMethod *method)
   {
   if (!method)
      return false; // end of recursion
   else if (method->convertToMethod()->isArchetypeSpecimen())
      return true; // Archetypes often call methods that are never called until the archetype is compiled
   else
      return cameFromArchetypeSpecimen(method->owningMethod());
   }
   
bool 
TR_J9EstimateCodeSize::adjustEstimateForStringCompression(TR_ResolvedMethod* method, int32_t& value, float factor)
   {   
   const uint16_t classNameLength = method->classNameLength();

   if ((classNameLength == 16 && !strncmp(method->classNameChars(), "java/lang/String", classNameLength)) ||
       (classNameLength == 22 && !strncmp(method->classNameChars(), "java/lang/StringBuffer", classNameLength)) ||
       (classNameLength == 23 && !strncmp(method->classNameChars(), "java/lang/StringBuilder", classNameLength)))
      {
      // A statistical analysis of the number of places certain methods got inlined yielded results which suggest that the
      // following recognized methods incurr several percent worth of increase in compile-time at no benefit to throughput.
      // As such we can save additional compile-time by not making adjustments to these methods.
      
      if (method->getRecognizedMethod() != TR::java_lang_String_regionMatches &&
          method->getRecognizedMethod() != TR::java_lang_String_regionMatches_bool &&
          method->getRecognizedMethod() != TR::java_lang_String_equals)
         {
         value *= factor;
      
         return true;
         }
      }
      
   return false;
   }

bool
TR_J9EstimateCodeSize::estimateCodeSize(TR_CallTarget *calltarget, TR_CallStack *prevCallStack, bool recurseDown)
   {
   if (realEstimateCodeSize(calltarget, prevCallStack, recurseDown))
      {
      if (_isLeaf && _realSize > 1)
         {
         heuristicTrace(tracer(),"Subtracting 1 from sizes because _isLeaf is true");
         --_realSize;
         --_optimisticSize;
         }
      return true;
      }

   return false;
   }


static TR::Node* getCallNode (TR::ResolvedMethodSymbol* methodSymbol, uint32_t bci)
   {
   for (TR::TreeTop* tt = methodSymbol->getFirstTreeTop(); tt; tt=tt->getNextTreeTop())
      {
      if (tt->getNode()->getNumChildren()>0 &&
          tt->getNode()->getFirstChild()->getOpCode().isCall() &&
          tt->getNode()->getFirstChild()->getByteCodeIndex() == bci)
         {
         return tt->getNode()->getFirstChild();
         }
      }

   return NULL;
   }


static bool hasArgInfoForChild (TR::Node* child)
   {

   return child->getOpCode().hasSymbolReference() &&
          child->getSymbolReference()->getSymbol()->isParm() &&
          child->getSymbolReference()->getSymbol()->getParmSymbol()->getFixedType();
   }


static void propagateReceiverInfoIfAvailable (TR::ResolvedMethodSymbol* methodSymbol, TR_CallSite* callsite,
                                              TR_PrexArgInfo * argInfo, TR_InlinerTracer *tracer)
   {

   //this implies we have some argInfo available
   TR_ASSERT(argInfo, "otherwise we shouldn't even peek");
   TR::Node* callNode = getCallNode(methodSymbol, callsite->_bcInfo.getByteCodeIndex());
   heuristicTrace(tracer, "ECS CSI -- trying to propagate receiver's info for callsite %p at %p", callsite, callNode);
   if (!callNode)
      return;

   //do not handle interface calls for now
   //as there is no obvious way to figure out the number of parameters
   if (callsite->_interfaceMethod)
      return;

   TR::Node* child = callNode->getChild(callNode->getFirstArgumentIndex());
   uint32_t numOfArgs = callsite->_initialCalleeMethod->numberOfParameters();

   if (callNode->getNumChildren()-callNode->getFirstArgumentIndex() != numOfArgs)
      {
      return;
      }

   if (hasArgInfoForChild(child))
      {
      heuristicTrace(tracer, "ECS CSI -- the receiver for callsite %p is also one of the caller's args");
      TR_PrexArgInfo *myPrexArgInfo = new (tracer->trHeapMemory()) TR_PrexArgInfo(numOfArgs, tracer->trMemory());
      TR_OpaqueClassBlock* argClass = (TR_OpaqueClassBlock*) child->getSymbolReference()->getSymbol()->getParmSymbol()->getFixedType();
      myPrexArgInfo->set(0, new (tracer->trHeapMemory()) TR_PrexArgument(TR_PrexArgument::ClassIsFixed, argClass));
      callsite->_ecsPrexArgInfo = myPrexArgInfo;
      }

   }


static void propagateArgs (TR::ResolvedMethodSymbol* methodSymbol, TR_CallSite* callsite,
                           TR_PrexArgInfo * argInfo, TR_InlinerTracer *tracer)
   {

   TR_ASSERT(argInfo, "otherwise we shouldn't even peek");
   TR::Node* callNode = getCallNode(methodSymbol, callsite->_bcInfo.getByteCodeIndex());
   heuristicTrace(tracer, "ECS CSI -- trying to propagate receiver's info for callsite %p at %p", callsite, callNode);
   if (!callNode)
      return;



   if (tracer->heuristicLevel())
      {
      heuristicTrace(tracer, "ECS CSI -- ArgInfo before args propagation");
      for (int i = 0; i < callsite->numTargets(); i++)
         tracer->dumpPrexArgInfo(callsite->getTarget(i)->_ecsPrexArgInfo);
      }


   for (int i = callNode->getFirstArgumentIndex(); i < callNode->getNumChildren(); i++)
      {
      TR::Node* child = callNode->getChild(i);
      if (hasArgInfoForChild(child)) //do we have any type info in caller for this parameter?
         {
         heuristicTrace(tracer, "ECS CSI -- arg %d at callsite %p matches caller's arg %d", i, callsite, child->getSymbolReference()->getSymbol()->getParmSymbol()->getOrdinal());

         for (int j = 0; j < callsite->numTargets(); j++)
            {
            TR_ASSERT(callsite->getTarget(j)->_ecsPrexArgInfo, "shouldn't be null! always initialize it in calltarget");

            //ILGen did something funky with this call, skip arg propagation
            if (callsite->getTarget(j)->_ecsPrexArgInfo->getNumArgs() != callNode->getNumChildren()-callNode->getFirstArgumentIndex())
               {
               continue;
               }

            TR_OpaqueClassBlock* argClass = (TR_OpaqueClassBlock*) child->getSymbolReference()->getSymbol()->getParmSymbol()->getFixedType();
            callsite->getTarget(j)->_ecsPrexArgInfo->set(i-callNode->getFirstArgumentIndex(), new (tracer->trHeapMemory()) TR_PrexArgument(TR_PrexArgument::ClassIsFixed, argClass));
            }
         }
      }

   if (tracer->heuristicLevel())
      {
      heuristicTrace(tracer, "ECS CSI -- ArgInfo after args propagation");
      for (int i = 0; i < callsite->numTargets(); i++)
         tracer->dumpPrexArgInfo(callsite->getTarget(i)->_ecsPrexArgInfo);
      }

   }

bool
TR_J9EstimateCodeSize::realEstimateCodeSize(TR_CallTarget *calltarget, TR_CallStack *prevCallStack, bool recurseDown)
   {
   TR_ASSERT(calltarget->_calleeMethod, "assertion failure");

   heuristicTrace(tracer(), "*** Depth %d: ECS CSI -- calltarget = %p , _ecsPrexArgInfo = %p",
      _recursionDepth, calltarget, calltarget->_ecsPrexArgInfo);



   if (tracer()->heuristicLevel() && calltarget->_ecsPrexArgInfo)
      {
      heuristicTrace(tracer(), "ECS CSI -- ArgInfo :");
      tracer()->dumpPrexArgInfo(calltarget->_ecsPrexArgInfo);
      }

   TR_InlinerDelimiter delimiter(tracer(), "estimateCodeSize");

   if (calltarget->_calleeMethod->numberOfExceptionHandlers() > 0)
      _hasExceptionHandlers = true;

   if (_aggressivelyInlineThrows)
      {
      TR_CatchBlockProfileInfo * catchInfo = TR_CatchBlockProfileInfo::get(comp(), calltarget->_calleeMethod);
      if (catchInfo)
         _throwCount += catchInfo->getThrowCounter();
      }

   //TR::Compilation * comp = _inliner->comp();

   char nameBuffer[1024];
   const char *callerName = NULL;
   if (tracer()->heuristicLevel())
      callerName = comp()->fej9()->sampleSignature(
            calltarget->_calleeMethod->getPersistentIdentifier(), nameBuffer,
            1024, comp()->trMemory());

   heuristicTrace(tracer(),
         "*** Depth %d: ECS to begin for target %p signature %s size assuming we can partially inline (optimistic size)  = %d total real size so far = %d sizeThreshold %d",
         _recursionDepth, calltarget, callerName, _optimisticSize, _realSize,
         _sizeThreshold);

   TR_ByteCodeInfo newBCInfo;
   newBCInfo.setDoNotProfile(0);
   TR::ResolvedMethodSymbol* methodSymbol = TR::ResolvedMethodSymbol::create(comp()->trHeapMemory(), calltarget->_calleeMethod, comp());
   if (_mayHaveVirtualCallProfileInfo)
      {
        if (!comp()->incInlineDepth(methodSymbol, calltarget->_myCallSite->_bcInfo, 0, NULL, !calltarget->_myCallSite->_isIndirectCall))
            {
            return false; //this is intentional
                          //calling returnCleanup here will result in assertion
                          //as incInlineDepth doesn't do anything
            }


      newBCInfo.setCallerIndex(comp()->getCurrentInlinedSiteIndex());
      }

   if( comp()->getVisitCount() > HIGH_VISIT_COUNT )
      {
      heuristicTrace(tracer(),"Depth %d: estimateCodeSize aborting due to high comp()->getVisitCount() of %d",_recursionDepth,comp()->getVisitCount());
      return returnCleanup(3);
      }

   if (_recursionDepth > MAX_ECS_RECURSION_DEPTH)
      {
      calltarget->_isPartialInliningCandidate = false;
      heuristicTrace(tracer(), "*** Depth %d: ECS end for target %p signature %s. Exceeded Recursion Depth", _recursionDepth, calltarget, callerName);
      return returnCleanup(1);
      }

   int32_t size = calltarget->_myCallSite->_isIndirectCall ? 5 : 0;

   TR_J9ByteCodeIterator bci(0, static_cast<TR_ResolvedJ9Method *> (calltarget->_calleeMethod), static_cast<TR_J9VMBase *> (comp()->fej9()), comp());

   bool inlineableCallExists = false;
   bool nonColdCallExists = false;
   bool callExists = false;

   int32_t maxIndex = bci.maxByteCodeIndex() + 5;

   flags8_t * flags = (flags8_t *) comp()->trMemory()->allocateStackMemory(
         maxIndex * sizeof(flags8_t));
   memset(flags, 0, maxIndex * sizeof(flags8_t));

   TR_CallSite * * callSites =
         (TR_CallSite * *) comp()->trMemory()->allocateStackMemory(maxIndex
               * sizeof(TR_CallSite *));
   memset(callSites, 0, maxIndex * sizeof(TR_CallSite *));

   int32_t *bcSizes = (int32_t *) comp()->trMemory()->allocateStackMemory(
         maxIndex * sizeof(int32_t));
   memset(bcSizes, 0, maxIndex * sizeof(int32_t));

   bool unresolvedSymbolsAreCold = comp()->notYetRunMeansCold();

   bool blockStart = true;
   TR_CallStack callStack(comp(), 0, calltarget->_calleeMethod, prevCallStack, 0);

   bool foundNewAllocation = false;
   bool hasThisCalls = false;
   bool thisOnStack = false;

   TR_PrexArgInfo* argsFromSymbol = TR_PrexArgInfo::buildPrexArgInfoForMethodSymbol(methodSymbol, tracer());

   if (!TR_PrexArgInfo::validateAndPropagateArgsFromCalleeSymbol(argsFromSymbol, calltarget->_ecsPrexArgInfo, tracer()))
   {
      heuristicTrace(tracer(), "*** Depth %d: ECS end for target %p signature %s. Incompatible arguments", _recursionDepth, calltarget, callerName);
      return returnCleanup(6);
   }

   NeedsPeekingHeuristic nph(calltarget, bci, methodSymbol, comp());

   //this might be a little bit too verbose, so let's hide the heuristic's output behind this env var
   static char *traceNeedsPeeking = feGetEnv("TR_traceNeedsPeekingHeuristic");
   if (traceNeedsPeeking)
      {
      nph.setTracer(tracer());
      }

   // PHASE 1:  Bytecode Iteration

   TR_J9ByteCode bc = bci.first(), nextBC;
   for (; bc != J9BCunknown; bc = bci.next())
      {
      nph.processByteCode();
      TR_ResolvedMethod * resolvedMethod;
      int32_t cpIndex;
      bool isVolatile, isPrivate, isUnresolvedInCP, resolved;
      TR::DataType type = TR::NoType;
      void * staticAddress;
      uint32_t fieldOffset;

      newBCInfo.setByteCodeIndex(bci.bcIndex());
      int32_t i = bci.bcIndex();

      if (blockStart) //&& calltarget->_calleeSymbol)
         {
         flags[i].set(bbStart);
         blockStart = false;
         foundNewAllocation = false;
         }

      if (bc == J9BCgenericReturn)
         {
         if (!calltarget->_calleeMethod->isSynchronized())
            size += 1;
         else
            size += bci.estimatedCodeSize();
         }
      else
         size += bci.estimatedCodeSize();

      switch (bc)
         {
         case J9BCificmpeq:
         case J9BCificmpne:
         case J9BCificmplt:
         case J9BCificmpge:
         case J9BCificmpgt:
         case J9BCificmple:
         case J9BCifacmpeq:
         case J9BCifacmpne:
         case J9BCifnull:
         case J9BCifnonnull:
         case J9BCifeq:
         case J9BCifne:
         case J9BCiflt:
         case J9BCifge:
         case J9BCifgt:
         case J9BCifle:
         case J9BCgoto:
         case J9BCgotow:
            flags[i].set(isBranch);
            flags[i + bci.relativeBranch()].set(bbStart);
            blockStart = true;
            break;
         case J9BCgenericReturn:
            flags[i].set(isBranch);
            blockStart = true;
            break;
         case J9BCnew:
         case J9BCnewarray:
         case J9BCanewarray:
         case J9BCmultianewarray:
            if (calltarget->_calleeSymbol)
               foundNewAllocation = true;
            flags[i].set(isUnsanitizeable);
            break;
         case J9BCathrow:
            _foundThrow = true;
            flags[i].set(isBranch);
            blockStart = true;
            if (!_aggressivelyInlineThrows)
               flags[i].set(isCold);
            flags[i].set(isUnsanitizeable);
            break;
         case J9BCtableswitch:
            {
            flags[i].set(isBranch);
            int32_t index = bci.defaultTargetIndex();
            flags[i + bci.nextSwitchValue(index)].set(bbStart);
            int32_t low = bci.nextSwitchValue(index);
            int32_t high = bci.nextSwitchValue(index) - low + 1;
            for (int32_t j = 0; j < high; ++j)
               flags[i + bci.nextSwitchValue(index)].set(bbStart);
            blockStart = true;
            flags[i].set(isUnsanitizeable);
            break;
            }
         case J9BClookupswitch:
            {
            flags[i].set(isBranch);
            int32_t index = bci.defaultTargetIndex();
            flags[i + bci.nextSwitchValue(index)].set(bbStart);
            int32_t tableSize = bci.nextSwitchValue(index);
            for (int32_t j = 0; j < tableSize; ++j)
               {
               index += 4; // match value
               flags[i + bci.nextSwitchValue(index)].set(bbStart);
               }
            blockStart = true;
            flags[i].set(isUnsanitizeable);
            break;
            }
         case J9BCinvokevirtual:
            {
            if (thisOnStack)
               hasThisCalls = true;
            cpIndex = bci.next2Bytes();
            auto calleeMethod = (TR_ResolvedJ9Method*)calltarget->_calleeMethod;
            resolvedMethod = calleeMethod->getResolvedPossiblyPrivateVirtualMethod(comp(), cpIndex, true, &isUnresolvedInCP);

            ///if (!resolvedMethod || isUnresolvedInCP || resolvedMethod->isCold(comp(), true))
            if ((isUnresolvedInCP && !resolvedMethod) || (resolvedMethod
                  && resolvedMethod->isCold(comp(), true)))
               {

               if(tracer()->heuristicLevel())
                  {
                  if(resolvedMethod)
                     {
                     heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s",_recursionDepth,i,tracer()->traceSignature(resolvedMethod));
                     }
                  else
                     {
                     TR_Method *meth = comp()->fej9()->createMethod(comp()->trMemory(), calltarget->_calleeMethod->containingClass(), cpIndex);
                     heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s",_recursionDepth,i,tracer()->traceSignature(meth));
                     }
                  }
               if (unresolvedSymbolsAreCold)
                  flags[i].set(isCold);
               _isLeaf = false;
               }
            }

            callExists = true;
            flags[i].set(isUnsanitizeable);
            break;
         case J9BCinvokespecial:
         case J9BCinvokespecialsplit:
            {
            if (thisOnStack)
               hasThisCalls = true;
            cpIndex = bci.next2Bytes();
            resolvedMethod = calltarget->_calleeMethod->getResolvedSpecialMethod(comp(), (bc == J9BCinvokespecialsplit)?cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG:cpIndex, &isUnresolvedInCP);
            bool isIndirectCall = false;
            bool isInterface = false;
            TR_Method *interfaceMethod = 0;
            TR::TreeTop *callNodeTreeTop = 0;
            TR::Node *parent = 0;
            TR::Node *callNode = 0;
            TR::ResolvedMethodSymbol *resolvedSymbol = 0;
            if (!resolvedMethod || isUnresolvedInCP || resolvedMethod->isCold(comp(), false))
               {
               if(tracer()->heuristicLevel())
                   {
                   if(resolvedMethod)
                      {
                      heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s",_recursionDepth,i,tracer()->traceSignature(resolvedMethod));
                      }
                   else
                      {
                      if (bc == J9BCinvokespecialsplit)
                         cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG;
                      TR_Method *meth = comp()->fej9()->createMethod(comp()->trMemory(), calltarget->_calleeMethod->containingClass(), cpIndex);
                      heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s",_recursionDepth,i,tracer()->traceSignature(meth));
                      }
                   }
               if (unresolvedSymbolsAreCold)
                  flags[i].set(isCold);
               _isLeaf = false;
               }
            }
            callExists = true;
            flags[i].set(isUnsanitizeable);
            break;
         case J9BCinvokestatic:
         case J9BCinvokestaticsplit:
            {
            cpIndex = bci.next2Bytes();
            resolvedMethod = calltarget->_calleeMethod->getResolvedStaticMethod(comp(), (bc == J9BCinvokestaticsplit)?cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG:cpIndex, &isUnresolvedInCP);
            bool isIndirectCall = false;
            bool isInterface = false;
            TR_Method *interfaceMethod = 0;
            TR::TreeTop *callNodeTreeTop = 0;
            TR::Node *parent = 0;
            TR::Node *callNode = 0;
            TR::ResolvedMethodSymbol *resolvedSymbol = 0;
            if (!resolvedMethod || isUnresolvedInCP || resolvedMethod->isCold(comp(), false))
               {
               if (unresolvedSymbolsAreCold)
                  flags[i].set(isCold);
               if(tracer()->heuristicLevel())
                   {
                   if(resolvedMethod)
                      heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s",_recursionDepth,i,tracer()->traceSignature(resolvedMethod));
                   else
                      {
                      if (bc == J9BCinvokestaticsplit)
                         cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG;
                      TR_Method *meth = comp()->fej9()->createMethod(comp()->trMemory(), calltarget->_calleeMethod->containingClass(), cpIndex);
                      heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s",_recursionDepth,i,tracer()->traceSignature(meth));
                      }
                   }
               }
            }
            callExists = true;
            flags[i].set(isUnsanitizeable);
            break;
         case J9BCinvokeinterface:
            cpIndex = bci.next2Bytes();
            flags[i].set(isUnsanitizeable);
            break;
         case J9BCgetfield:
            resolved = calltarget->_calleeMethod->fieldAttributes(comp(), bci.next2Bytes(), &fieldOffset, &type, &isVolatile, 0, &isPrivate, false, &isUnresolvedInCP, false);
            if (!resolved || isUnresolvedInCP)
               {
               if (unresolvedSymbolsAreCold)
                  flags[i].set(isCold);
               if (!resolved)
                  _isLeaf = false;
               }
            if (isInExceptionRange(calltarget->_calleeMethod, i))
               flags[i].set(isUnsanitizeable);
            break;
         case J9BCputfield:
            resolved = calltarget->_calleeMethod->fieldAttributes(comp(), bci.next2Bytes(), &fieldOffset, &type, &isVolatile, 0, &isPrivate, true, &isUnresolvedInCP, false);
            if (!resolved || isUnresolvedInCP)
               {
               if (unresolvedSymbolsAreCold)
                  flags[i].set(isCold);
               if (!resolved)
                  _isLeaf = false;
               }
            flags[i].set(isUnsanitizeable);
            break;
         case J9BCgetstatic:
            resolved = calltarget->_calleeMethod->staticAttributes(comp(), bci.next2Bytes(), &staticAddress, &type, &isVolatile, 0, &isPrivate, false, &isUnresolvedInCP, false);
            if (!resolved || isUnresolvedInCP)
               {
               if (unresolvedSymbolsAreCold)
                  flags[i].set(isCold);
               if (!resolved)
                  _isLeaf = false;
               }
            if (isInExceptionRange(calltarget->_calleeMethod, i))
               flags[i].set(isUnsanitizeable);
            break;
         case J9BCputstatic:
            resolved = calltarget->_calleeMethod->staticAttributes(comp(), bci.next2Bytes(), &staticAddress, &type, &isVolatile, 0, &isPrivate, true, &isUnresolvedInCP, false);
            if (!resolved || isUnresolvedInCP)
               {
               if (unresolvedSymbolsAreCold)
                  flags[i].set(isCold);
               if (!resolved)
                  _isLeaf = false;
               }
            flags[i].set(isUnsanitizeable);
            break;
         case J9BCaload0:
            if (calltarget->_myCallSite->_isIndirectCall)
               thisOnStack = true;
            break;
         case J9BCiastore:
         case J9BClastore:
         case J9BCfastore:
         case J9BCdastore:
         case J9BCaastore:
         case J9BCbastore:
         case J9BCcastore:
         case J9BCsastore: //array stores can change the global state - hence unsanitizeable
            flags[i].set(isUnsanitizeable);
            break;
         case J9BCiaload:
         case J9BClaload:
         case J9BCfaload:
         case J9BCdaload:
         case J9BCaaload:
         case J9BCbaload:
         case J9BCcaload:
         case J9BCsaload:
         case J9BCarraylength: //array accesses are ok as long as we don't catch exception
         case J9BCidiv:
         case J9BCldiv:
         case J9BCfdiv:
         case J9BCddiv:
         case J9BCirem:
         case J9BClrem:
         case J9BCfrem:
         case J9BCdrem:
         case J9BCcheckcast:
         case J9BCinstanceof:
         case J9BCasyncCheck:
            if (isInExceptionRange(calltarget->_calleeMethod, i))
               flags[i].set(isUnsanitizeable);
            break;
         case J9BCinvokedynamic:
         case J9BCinvokehandle:
         case J9BCinvokehandlegeneric:
            // TODO:JSR292: Use getResolvedHandleMethod
         case J9BCmonitorenter:
         case J9BCmonitorexit:
         case J9BCunknown:
            flags[i].set(isUnsanitizeable);
            break;
         default:
         	break;
         }

      if (flags[i].testAny(isUnsanitizeable))
         debugTrace(tracer(),"BC at index %d is unsanitizeable.", i);
      else if (flags[i].testAny(isCold))
         debugTrace(tracer(),"BC at index %d is cold.", i);
      else
         debugTrace(tracer(),"BC iteration at index %d.", i); //only print this index if we are debugging

      bcSizes[i] = size;
      }
      
   auto sizeBeforeAdjustment = size;
      
   if (adjustEstimateForStringCompression(calltarget->_calleeMethod, size, STRING_COMPRESSION_ADJUSTMENT_FACTOR))
      {
      heuristicTrace(tracer(), "*** Depth %d: Adjusting size for %s because of string compression from %d to %d", _recursionDepth, callerName, sizeBeforeAdjustment, size);
      }

   calltarget->_fullSize = size;

   if (calltarget->_calleeSymbol)
      {
      TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "inliner/%s/estimatedBytecodeSize/%d", calltarget->_calleeSymbol->signature(comp()->trMemory()), calltarget->_fullSize));
      }

   bool wasPeekingSuccessfull = false;

   if (nph.doPeeking() && recurseDown)
      {

      heuristicTrace(tracer(), "*** Depth %d: ECS CSI -- needsPeeking is true for calltarget %p",
      _recursionDepth, calltarget);

      methodSymbol = TR::ResolvedMethodSymbol::create(comp()->trHeapMemory(),calltarget->_calleeMethod,comp());
      bool ilgenSuccess = (NULL != methodSymbol->getResolvedMethod()->genMethodILForPeekingEvenUnderMethodRedefinition(methodSymbol, comp(), false, NULL));
      if (ilgenSuccess)
         {
         heuristicTrace(tracer(), "*** Depth %d: ECS CSI -- peeking was successfull for calltarget %p", _recursionDepth, calltarget);
         calltarget->_ecsPrexArgInfo->clearArgInfoForNonInvariantArguments(methodSymbol, tracer());
         wasPeekingSuccessfull = true;
         }
      }

   /********* PHASE 2: Generate CFG **********/

   heuristicTrace(tracer(),"--- Done Iterating over Bytecodes in call to %s.  size = %d _recursionDepth = %d _optimisticSize = %d _realSize = %d _sizeThreshold = %d",callerName, size, _recursionDepth, _optimisticSize, _realSize, _sizeThreshold);

   if (hasThisCalls && calltarget->_calleeSymbol)
      calltarget->_calleeSymbol->setHasThisCalls(true);

   TR_Queue<TR::Block> callBlocks(comp()->trMemory());

   //if (callExists)
   if (true) // Need to run the below code to inline partially anyway (even if there are no calls in this callee)
      {
      TR_Array<TR_J9ByteCodeIterator::TryCatchInfo> tryCatchInfo(
            comp()->trMemory(),
            calltarget->_calleeMethod->numberOfExceptionHandlers(), true,
            stackAlloc);

      int32_t i;
      for (i = calltarget->_calleeMethod->numberOfExceptionHandlers() - 1; i
            >= 0; --i)
         {
         int32_t start, end, type;
         int32_t handler = calltarget->_calleeMethod->exceptionData(i, &start,
               &end, &type);

         flags[start].set(bbStart);
         flags[end + 1].set(bbStart);
         flags[handler].set(bbStart);

         tryCatchInfo[i].initialize((uint16_t) start, (uint16_t) end,
               (uint16_t) handler, (uint32_t) type);
         }

      TR::CFG cfg(comp(), calltarget->_calleeSymbol);
      cfg.setStartAndEnd(new (comp()->trStackMemory()) TR::Block(
            TR::TreeTop::create(comp(), TR::Node::createOnStack(NULL,
                  TR::BBStart, 0)), TR::TreeTop::create(comp(),
                  TR::Node::createOnStack(NULL, TR::BBEnd, 0)),
            comp()->trMemory()), new (comp()->trStackMemory()) TR::Block(
            TR::TreeTop::create(comp(), TR::Node::createOnStack(NULL,
                  TR::BBStart, 0)), TR::TreeTop::create(comp(),
                  TR::Node::createOnStack(NULL, TR::BBEnd, 0)),
            comp()->trMemory()));

      cfg.getStart()->asBlock()->getEntry()->join(
            cfg.getStart()->asBlock()->getExit());
      cfg.getEnd()->asBlock()->getEntry()->join(
            cfg.getEnd()->asBlock()->getExit());
      cfg.getStart()->setNumber(cfg.getNextNodeNumber());
      cfg.allocateNodeNumber();
      cfg.getEnd()->setNumber(cfg.getNextNodeNumber());
      cfg.allocateNodeNumber();

      cfg.getEnd()->asBlock()->setIsEndBlock();

      TR::Block * currentBlock = cfg.getStart()->asBlock();
      currentBlock->setJ9EstimateCodeSizeMethod(calltarget->_calleeMethod);
      currentBlock->setBlockBCIndex(0);
      cfg.getStart()->asBlock()->setJ9EstimateCodeSizeMethod(calltarget->_calleeMethod);
      cfg.getStart()->asBlock()->setBlockBCIndex(0);

      int32_t endNodeIndex = bci.maxByteCodeIndex() - 1;
      if (endNodeIndex < 0)
         {
         debugTrace(tracer(), "MaxByteCodeIndex <= 0, setting BC index for end node to 0.");
         endNodeIndex = 0;
         }

      setupNode(cfg.getStart()->asBlock()->getEntry()->getNode(), 0,
            calltarget->_calleeMethod, comp());
      setupNode(cfg.getStart()->asBlock()->getExit()->getNode(), 0,
            calltarget->_calleeMethod, comp());
      setupNode(cfg.getEnd()->asBlock()->getEntry()->getNode(),
            endNodeIndex, calltarget->_calleeMethod, comp());
      setupNode(cfg.getEnd()->asBlock()->getExit()->getNode(),
            endNodeIndex, calltarget->_calleeMethod, comp());

      TR::Block * * blocks =
            (TR::Block * *) comp()->trMemory()->allocateStackMemory(maxIndex
                  * sizeof(TR::Block *));
      memset(blocks, 0, maxIndex * sizeof(TR::Block *));

      debugTrace(tracer(),"PECS: startblock %p %d endblock %p %d",cfg.getStart()->asBlock(), cfg.getStart()->getNumber(), cfg.getEnd()->asBlock(), cfg.getEnd()->getNumber());

      bool addFallThruEdge = true;

      debugTrace(tracer(),"PECS: iterating over bc indexes in CFG creation. maxIndex =%d", maxIndex);
      int32_t blockStartSize = 0;
      int32_t startIndex = 0;
      for (i = 0; i < maxIndex; ++i)
         {
         if (flags[i].testAny(bbStart))
            {
            debugTrace(tracer(),"Calling getBlock.  blocks[%d] = %p", i, blocks[i]);
            TR::Block * newBlock = getBlock(comp(), blocks,
                  calltarget->_calleeMethod, i, cfg);

            if (i != startIndex)
               {
               currentBlock->setBlockSize(bcSizes[i] - blockStartSize);
               if (cfg.getMethodSymbol())
                  cfg.getMethodSymbol()->addProfilingOffsetInfo(currentBlock->getEntry()->getNode()->getByteCodeIndex(), currentBlock->getEntry()->getNode()->getByteCodeIndex() + currentBlock->getBlockSize());
               }

            if (addFallThruEdge)
               {
               debugTrace(tracer(),"adding a fallthrough edge between block %p %d and %p %d", currentBlock, currentBlock->getNumber(), newBlock, newBlock->getNumber());

               debugTrace(tracer(),"joining nodes between blocks %p %d and %p %d", currentBlock, currentBlock->getNumber(), newBlock, newBlock->getNumber());
               currentBlock->getExit()->join(newBlock->getEntry());

               cfg.addEdge(currentBlock, newBlock, stackAlloc);
               addFallThruEdge = true;
               }
            else
               {
               addFallThruEdge = true;
               }
            currentBlock = newBlock;

            startIndex = i;
            blockStartSize = bcSizes[i];
            }

         if (flags[i].testAny(isCold))
            {
            partialTrace(tracer(), "Setting block %p[%d] blocks[%d]=%p as cold because bytecode %d was identified as cold",currentBlock, currentBlock->getNumber(), i, blocks[i], i);
            currentBlock->setIsCold();
            currentBlock->setFrequency(0);
            }
         if (flags[i].testAny(isUnsanitizeable))
            {
            partialTrace(tracer(), "Setting unsanitizeable flag on block %p[%d] blocks[%d]=%p",currentBlock, currentBlock->getNumber(), i, blocks[i]);
            currentBlock->setIsUnsanitizeable();
            }

         if (callSites[i])
            {
            for (int kk = 0; kk < callSites[i]->numTargets(); kk++)
               callSites[i]->getTarget(kk)->_originatingBlock = currentBlock;
            }

         if (flags[i].testAny(isBranch))
            {
            if (startIndex != i)
               {
               currentBlock->setBlockSize(bcSizes[i] - blockStartSize);
               if (cfg.getMethodSymbol())
                  cfg.getMethodSymbol()->addProfilingOffsetInfo(currentBlock->getEntry()->getNode()->getByteCodeIndex(), currentBlock->getEntry()->getNode()->getByteCodeIndex() + currentBlock->getBlockSize());
               }
            else
               {
               currentBlock->setBlockSize(1); // if there startIndex is the same as the current index then the block consists only of a branch
               if (cfg.getMethodSymbol())
                  cfg.getMethodSymbol()->addProfilingOffsetInfo(currentBlock->getEntry()->getNode()->getByteCodeIndex(), currentBlock->getEntry()->getNode()->getByteCodeIndex() + currentBlock->getBlockSize());
               }

            bci.setIndex(i);
            switch (bc = bci.current())
               {
               case J9BCificmpeq:
               case J9BCificmpne:
               case J9BCificmplt:
               case J9BCificmpge:
               case J9BCificmpgt:
               case J9BCificmple:
               case J9BCifacmpeq:
               case J9BCifacmpne:
               case J9BCifeq:
               case J9BCifne:
               case J9BCiflt:
               case J9BCifge:
               case J9BCifgt:
               case J9BCifle:
               case J9BCifnull:
               case J9BCifnonnull:
                  {
                  debugTrace(tracer(),"if branch.i = %d adding edge between blocks %p %d and %p %d",
                                       i, currentBlock, currentBlock->getNumber(), getBlock(comp(), blocks, calltarget->_calleeMethod, i+ bci.relativeBranch(), cfg),
                                       getBlock(comp(), blocks, calltarget->_calleeMethod, i + bci.relativeBranch(), cfg)->getNumber());

                  setupLastTreeTop(currentBlock, bc, i, getBlock(comp(), blocks, calltarget->_calleeMethod, i + bci.relativeBranch(), cfg), calltarget->_calleeMethod, comp());
                  cfg.addEdge(currentBlock, getBlock(comp(), blocks,
                        calltarget->_calleeMethod, i + bci.relativeBranch(),
                        cfg), stackAlloc);
                  addFallThruEdge = true;
                  break;
                  }
               case J9BCgoto:
               case J9BCgotow:
                  setupLastTreeTop(currentBlock, bc, i, getBlock(comp(), blocks, calltarget->_calleeMethod, i + bci.relativeBranch(), cfg), calltarget->_calleeMethod, comp());
                  cfg.addEdge(currentBlock, getBlock(comp(), blocks, calltarget->_calleeMethod, i + bci.relativeBranch(), cfg), stackAlloc);
                  addFallThruEdge = false;
                  break;
               case J9BCgenericReturn:
               case J9BCathrow:
                  setupLastTreeTop(currentBlock, bc, i, cfg.getEnd()->asBlock(), calltarget->_calleeMethod, comp());
                  cfg.addEdge(currentBlock, cfg.getEnd(), stackAlloc);
                  addFallThruEdge = false;
                  break;
               case J9BCtableswitch:
                  {
                  int32_t index = bci.defaultTargetIndex();
                  TR::Block *defaultBlock = getBlock(comp(), blocks,
                        calltarget->_calleeMethod, i + bci.nextSwitchValue(
                              index), cfg);
                  setupLastTreeTop(currentBlock, bc, i, defaultBlock,
                        calltarget->_calleeMethod, comp());
                  cfg.addEdge(currentBlock, defaultBlock, stackAlloc);
                  int32_t low = bci.nextSwitchValue(index);
                  int32_t high = bci.nextSwitchValue(index) - low + 1;
                  for (int32_t j = 0; j < high; ++j)
                     cfg.addEdge(currentBlock, getBlock(comp(), blocks,
                           calltarget->_calleeMethod, i + bci.nextSwitchValue(
                                 index), cfg), stackAlloc);
                  addFallThruEdge = false;
                  break;
                  }
               case J9BClookupswitch:
                  {
                  int32_t index = bci.defaultTargetIndex();
                  TR::Block *defaultBlock = getBlock(comp(), blocks,
                        calltarget->_calleeMethod, i + bci.nextSwitchValue(
                              index), cfg);
                  setupLastTreeTop(currentBlock, bc, i, defaultBlock,
                        calltarget->_calleeMethod, comp());
                  cfg.addEdge(currentBlock, defaultBlock, stackAlloc);
                  int32_t tableSize = bci.nextSwitchValue(index);
                  for (int32_t j = 0; j < tableSize; ++j)
                     {
                     index += 4; // match value
                     cfg.addEdge(currentBlock, getBlock(comp(), blocks,
                           calltarget->_calleeMethod, i + bci.nextSwitchValue(
                                 index), cfg), stackAlloc);
                     }
                  addFallThruEdge = false;
                  break;
                  }
               default:
               	break;
               }
            }
         //      printf("Iterating through sizes array.  bcSizes[%d] = %d maxIndex = %d\n",i,bcSizes[i],maxIndex);
         }

      for (i = 0; i < (int32_t) tryCatchInfo.size(); ++i)
         {
         TR_J9ByteCodeIterator::TryCatchInfo * handlerInfo = &tryCatchInfo[i];

         blocks[handlerInfo->_handlerIndex]->setHandlerInfoWithOutBCInfo(
               handlerInfo->_catchType, 0, handlerInfo->_handlerIndex,
               calltarget->_calleeMethod, comp());

         for (int32_t j = handlerInfo->_startIndex; j <= handlerInfo->_endIndex; ++j)
            if (blocks[j])
               cfg.addExceptionEdge(blocks[j], blocks[handlerInfo->_handlerIndex], stackAlloc);
         }

      // Adjust call frequency for unknown or direct calls, for which we don't get profiling information
      //
      TR_ValueProfileInfoManager * profileManager = TR_ValueProfileInfoManager::get(comp());
      bool callGraphEnabled = !comp()->getOption(TR_DisableCallGraphInlining);//profileManager->isCallGraphProfilingEnabled(comp());
      if (!_inliner->firstPass())
         callGraphEnabled = false; // TODO: Work out why this doesn't function properly on subsequent passes
      if (callGraphEnabled && recurseDown)
         {
         TR_OpaqueMethodBlock *method = calltarget->_myCallSite->_callerResolvedMethod->getPersistentIdentifier();
         uint32_t bcIndex = calltarget->_myCallSite->_bcInfo.getByteCodeIndex();
         int32_t callCount = profileManager->getCallGraphProfilingCount(method,
               bcIndex, comp());
         cfg._calledFrequency = callCount;

         if (callCount <= 0 && _lastCallBlockFrequency > 0)
            cfg._calledFrequency = _lastCallBlockFrequency;

         heuristicTrace(tracer(),
               "Depth %d: Setting called count for caller index %d, bytecode index %d of %d", _recursionDepth,
               calltarget->_myCallSite->_bcInfo.getCallerIndex(),
               calltarget->_myCallSite->_bcInfo.getByteCodeIndex(), callCount);
         }
      else if (callGraphEnabled)
         {
         cfg._calledFrequency = 10000;
         }

      cfg.propagateColdInfo(callGraphEnabled); // propagate coldness but also generate frequency information
      // for blocks if call graph profiling is enabled

      if (tracer()->heuristicLevel())
         {
         heuristicTrace(tracer(), "After propagating the coldness info\n");
         heuristicTrace(tracer(), "<cfg>");
         for (TR::CFGNode* node = cfg.getFirstNode(); node; node = node->getNext())
            {
            comp()->findOrCreateDebug()->print(comp()->getOutFile(), node, 6);
            }
         heuristicTrace(tracer(), "</cfg>");
         }


      TR_prevArgs pca;
      TR::Block *currentInlinedBlock = NULL;
      TR_J9ByteCode bc = bci.first(), nextBC;
      for (; bc != J9BCunknown; bc = bci.next())
         {
         TR_ResolvedMethod * resolvedMethod;
         int32_t cpIndex;
         bool isVolatile, isPrivate, isUnresolvedInCP, resolved;
         TR::DataType type = TR::NoType;
         void * staticAddress;
         uint32_t fieldOffset;

         newBCInfo.setByteCodeIndex(bci.bcIndex());
         int32_t i = bci.bcIndex();

         if (flags[i].testAny(bbStart))
            {
            currentInlinedBlock = getBlock(comp(), blocks,
                  calltarget->_calleeMethod, i, cfg);
            debugTrace(tracer(),"Found current block %p, number %d\n", currentInlinedBlock, (currentInlinedBlock) ? currentInlinedBlock->getNumber() : -1);
            }

         switch (bc)
            {
            case J9BCinvokevirtual:
               {
               if (thisOnStack)
                  hasThisCalls = true;
               cpIndex = bci.next2Bytes();
               auto calleeMethod = (TR_ResolvedJ9Method*)calltarget->_calleeMethod;
               resolvedMethod = calleeMethod->getResolvedPossiblyPrivateVirtualMethod(comp(), cpIndex, true, &isUnresolvedInCP);
               bool isIndirectCall =
                  resolvedMethod == NULL
                  || (!resolvedMethod->isFinal() && !resolvedMethod->isPrivate());
               bool isInterface = false;
               TR_Method *interfaceMethod = 0;
               TR::TreeTop *callNodeTreeTop = 0;
               TR::Node *parent = 0;
               TR::Node *callNode = 0;
               TR::ResolvedMethodSymbol *resolvedSymbol = 0;

               ///if (!resolvedMethod || isUnresolvedInCP || resolvedMethod->isCold(comp(), true))
               if ((isUnresolvedInCP && !resolvedMethod) || (resolvedMethod && resolvedMethod->isCold(comp(), true)))
                  {
                  if(tracer()->heuristicLevel())
                      {
                      if(resolvedMethod)
                         heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s",_recursionDepth,i,tracer()->traceSignature(resolvedMethod));
                      else
                         {
                         TR_Method *meth = comp()->fej9()->createMethod(comp()->trMemory(), calltarget->_calleeMethod->containingClass(), cpIndex);
                         heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s",_recursionDepth,i,tracer()->traceSignature(meth));
                         }
                      }
                  if (unresolvedSymbolsAreCold)
                     flags[i].set(isCold);
                  _isLeaf = false;
                  }
               else if (resolvedMethod)
                  {
                  bool allconsts= false;


                  heuristicTrace(tracer(),"numberOfExplicitParameters = %d  pca.getNumPrevConstArgs = %d\n",resolvedMethod->numberOfExplicitParameters() ,pca.getNumPrevConstArgs(resolvedMethod->numberOfExplicitParameters()));
                  if ( resolvedMethod->numberOfExplicitParameters() > 0 && resolvedMethod->numberOfExplicitParameters() <= pca.getNumPrevConstArgs(resolvedMethod->numberOfExplicitParameters()))
                     allconsts = true;


                  TR_CallSite *callsite;

                  if (resolvedMethod->convertToMethod()->isArchetypeSpecimen() && resolvedMethod->getMethodHandleLocation())
                     {
                     callsite = new (comp()->trHeapMemory()) TR_J9MethodHandleCallSite( callStack._method, callNodeTreeTop,   parent,
                                                                                    callNode, interfaceMethod, resolvedMethod->classOfMethod(),
                                                                                    (int32_t) resolvedMethod->virtualCallSelector(cpIndex), cpIndex, resolvedMethod,
                                                                                    resolvedSymbol, isIndirectCall, isInterface, newBCInfo, comp(),
                                                                                    _recursionDepth, allconsts);
                     }
                  else if (resolvedMethod->getRecognizedMethod() == TR::java_lang_invoke_MethodHandle_invokeExact)
                     {
                     callsite = new (comp()->trHeapMemory()) TR_J9MutableCallSite( callStack._method, callNodeTreeTop,   parent,
                                                                  callNode, interfaceMethod, resolvedMethod->classOfMethod(),
                                                                  (int32_t) resolvedMethod->virtualCallSelector(cpIndex), cpIndex, resolvedMethod,
                                                                  resolvedSymbol, isIndirectCall, isInterface, newBCInfo, comp(),
                                                                  _recursionDepth, allconsts);
                     }
                  else if (isIndirectCall)
                     {
                     callsite = new (comp()->trHeapMemory()) TR_J9VirtualCallSite( callStack._method, callNodeTreeTop, parent,
                                                                                    callNode, interfaceMethod, resolvedMethod->classOfMethod(),
                                                                                    (int32_t) resolvedMethod->virtualCallSelector(cpIndex), cpIndex, resolvedMethod,
                                                                                    resolvedSymbol, isIndirectCall, isInterface, newBCInfo, comp(),
                                                                                    _recursionDepth, allconsts);

                     }
                        else
                            {
                            callsite = new (comp()->trHeapMemory()) TR_DirectCallSite( callStack._method, callNodeTreeTop, parent,
                                                                                    callNode, interfaceMethod, resolvedMethod->classOfMethod(),
                                                                                    (int32_t) resolvedMethod->virtualCallSelector(cpIndex), cpIndex, resolvedMethod,
                                                                                    resolvedSymbol, isIndirectCall, isInterface, newBCInfo, comp(),
                                                                                    _recursionDepth, allconsts);

                            }


                  if(tracer()->debugLevel())
                     {
                     pca.printIndexes(comp());
                     }
                  if(pca.isArgAtIndexReceiverObject(resolvedMethod->numberOfExplicitParameters()))
                     {
                     //heuristicTrace(tracer(),"Arg at index %d is receiver object.  Propagating prexarginfo",resolvedMethod->numberOfExplicitParameters());
                     //callsite->_ecsPrexArgInfo = calltarget->_ecsPrexArgInfo;
                     }

                  TR_PrexArgInfo *argInfo = calltarget->_ecsPrexArgInfo;

                  if (wasPeekingSuccessfull)
                     {
                     TR_PrexArgInfo::propagateReceiverInfoIfAvailable(methodSymbol, callsite, argInfo, tracer());
                     if (tracer()->heuristicLevel())
                        {
                        alwaysTrace(tracer(), "propagateReceiverInfoIfAvailable :");
                        if (callsite->_ecsPrexArgInfo)
                           tracer()->dumpPrexArgInfo(callsite->_ecsPrexArgInfo);
                        }
                     }

                  callsite->_callerBlock = currentInlinedBlock;
                  if (isInlineable(&callStack, callsite))
                     {
                     callSites[i] = callsite;
                     inlineableCallExists = true;

                  if (wasPeekingSuccessfull)
                     {
                     TR_PrexArgInfo::propagateArgsFromCaller(methodSymbol, callsite, argInfo, tracer());
                     if (tracer()->heuristicLevel())
                        {
                        alwaysTrace(tracer(), "propagateArgs :");
                        if (callsite->numTargets() && callsite->getTarget(0)->_ecsPrexArgInfo)
                           tracer()->dumpPrexArgInfo(callsite->getTarget(0)->_ecsPrexArgInfo);
                        }
                     }

                     if (!currentInlinedBlock->isCold())
                          nonColdCallExists = true;
                     }
                  else
                     //support counters
                     calltarget->addDeadCallee(callsite);
                  }
               }
               flags[i].set(isUnsanitizeable);
               break;

            case J9BCinvokespecial:
            case J9BCinvokespecialsplit:
               {
               if (thisOnStack)
                  hasThisCalls = true;
               cpIndex = bci.next2Bytes();
               resolvedMethod = calltarget->_calleeMethod->getResolvedSpecialMethod(comp(), (bc == J9BCinvokespecialsplit)?cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG:cpIndex, &isUnresolvedInCP);
               bool isIndirectCall = false;
               bool isInterface = false;
               TR_Method *interfaceMethod = 0;
               TR::TreeTop *callNodeTreeTop = 0;
               TR::Node *parent = 0;
               TR::Node *callNode = 0;
               TR::ResolvedMethodSymbol *resolvedSymbol = 0;
               if (!resolvedMethod || isUnresolvedInCP || resolvedMethod->isCold(comp(), false))
                  {
                  if(tracer()->heuristicLevel())
                      {
                      if(resolvedMethod)
                         heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s",_recursionDepth,i,tracer()->traceSignature(resolvedMethod));
                      else
                         {
                         if (bc == J9BCinvokespecialsplit)
                            cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG;
                         TR_Method *meth = comp()->fej9()->createMethod(comp()->trMemory(), calltarget->_calleeMethod->containingClass(), cpIndex);
                         heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s",_recursionDepth,i,tracer()->traceSignature(meth));
                         }
                      }
                  if (unresolvedSymbolsAreCold)
                     flags[i].set(isCold);
                  _isLeaf = false;
                  }
               else
                  {
                  bool allconsts= false;

                  heuristicTrace(tracer(),"numberOfExplicitParameters = %d  pca.getNumPrevConstArgs = %d\n",resolvedMethod->numberOfExplicitParameters() ,pca.getNumPrevConstArgs(resolvedMethod->numberOfExplicitParameters()));
                  if ( resolvedMethod->numberOfExplicitParameters() > 0 && resolvedMethod->numberOfExplicitParameters() <= pca.getNumPrevConstArgs(resolvedMethod->numberOfExplicitParameters()))
                     allconsts = true;

                  TR_CallSite *callsite = new (comp()->trHeapMemory()) TR_DirectCallSite( callStack._method, callNodeTreeTop, parent,
                                                                                    callNode, interfaceMethod, resolvedMethod->classOfMethod(), -1, cpIndex,
                                                                                    resolvedMethod, resolvedSymbol, isIndirectCall, isInterface, newBCInfo, comp(),
                                                                                    _recursionDepth, allconsts);

                  TR_PrexArgInfo *argInfo = calltarget->_ecsPrexArgInfo;
                  callsite->_callerBlock = currentInlinedBlock;

                  if (isInlineable(&callStack, callsite))
                     {

                     if (wasPeekingSuccessfull)
                        {
                        TR_PrexArgInfo::propagateArgsFromCaller(methodSymbol, callsite, argInfo, tracer());
                        }

                     callSites[i] = callsite;
                     inlineableCallExists = true;
                     if (!currentInlinedBlock->isCold())
                          nonColdCallExists = true;
                     }
                  else
                     calltarget->addDeadCallee(callsite);
                  }
               }
               flags[i].set(isUnsanitizeable);
               break;

            case J9BCinvokestatic:
            case J9BCinvokestaticsplit:
               {
               cpIndex = bci.next2Bytes();
               resolvedMethod = calltarget->_calleeMethod->getResolvedStaticMethod(comp(), (bc == J9BCinvokestaticsplit)?cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG:cpIndex, &isUnresolvedInCP);
               bool isIndirectCall = false;
               bool isInterface = false;
               TR_Method *interfaceMethod = 0;
               TR::TreeTop *callNodeTreeTop = 0;
               TR::Node *parent = 0;
               TR::Node *callNode = 0;
               TR::ResolvedMethodSymbol *resolvedSymbol = 0;
               if (!resolvedMethod || isUnresolvedInCP || resolvedMethod->isCold(comp(), false))
                  {
                  if (unresolvedSymbolsAreCold)
                     flags[i].set(isCold);
                  if(tracer()->heuristicLevel())
                      {
                      if(resolvedMethod)
                         heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s",_recursionDepth,i,tracer()->traceSignature(resolvedMethod));
                      else
                         {
                         if (bc == J9BCinvokestaticsplit)
                            cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG;
                         TR_Method *meth = comp()->fej9()->createMethod(comp()->trMemory(), calltarget->_calleeMethod->containingClass(), cpIndex);
                         heuristicTrace(tracer(), "Depth %d: Call at bc index %d is Cold.  Not searching for targets. Signature %s",_recursionDepth,i,tracer()->traceSignature(meth));
                         }
                      }
                  }
               else
                  {
                  bool allconsts= false;

                  heuristicTrace(tracer(),"numberOfExplicitParameters = %d  pca.getNumPrevConstArgs = %d\n",resolvedMethod->numberOfExplicitParameters() ,pca.getNumPrevConstArgs(resolvedMethod->numberOfExplicitParameters()));
                  if ( resolvedMethod->numberOfExplicitParameters() > 0 && resolvedMethod->numberOfExplicitParameters() <= pca.getNumPrevConstArgs(resolvedMethod->numberOfExplicitParameters()))
                     allconsts = true;

                  TR_CallSite *callsite = new (comp()->trHeapMemory()) TR_DirectCallSite( callStack._method, callNodeTreeTop, parent, callNode, interfaceMethod,
                                                                                    resolvedMethod->classOfMethod(), -1, cpIndex, resolvedMethod, resolvedSymbol,
                                                                                    isIndirectCall, isInterface, newBCInfo, _inliner->comp(),
                                                                                    _recursionDepth, allconsts);

                  TR_PrexArgInfo *argInfo = calltarget->_ecsPrexArgInfo;
                  callsite->_callerBlock = currentInlinedBlock;
                  if (isInlineable(&callStack, callsite))
                     {

                     if (wasPeekingSuccessfull)
                        {
                        TR_PrexArgInfo::propagateArgsFromCaller(methodSymbol, callsite, argInfo, tracer());
                        }

                     callSites[i] = callsite;
                     inlineableCallExists = true;
                     if (!currentInlinedBlock->isCold())
                        nonColdCallExists = true;
                     }
                  else
                     calltarget->addDeadCallee(callsite);
                  }
               }
               flags[i].set(isUnsanitizeable);
               break;

            case J9BCinvokeinterface:
               {
               cpIndex = bci.next2Bytes();

               auto calleeMethod = (TR_ResolvedJ9Method*)calltarget->_calleeMethod;
               resolvedMethod = calleeMethod->getResolvedImproperInterfaceMethod(comp(), cpIndex);
               bool isIndirectCall = true;
               bool isInterface = true;
               if (resolvedMethod != NULL)
                  {
                  isInterface = false;
                  isIndirectCall =
                     !resolvedMethod->isPrivate()
                     && !resolvedMethod->convertToMethod()->isFinalInObject();
                  }

               TR_Method * interfaceMethod = NULL;
               if (isInterface)
                  interfaceMethod = comp()->fej9()->createMethod( comp()->trMemory(), calltarget->_calleeMethod->containingClass(), cpIndex);

               TR::TreeTop *callNodeTreeTop = 0;
               TR::Node *parent = 0;
               TR::Node *callNode = 0;
               TR::ResolvedMethodSymbol *resolvedSymbol = 0;

               uint32_t explicitParams = 0;
               if (isInterface)
                  explicitParams = interfaceMethod->numberOfExplicitParameters();
               else
                  explicitParams = resolvedMethod->numberOfExplicitParameters();

               bool allconsts= false;
               heuristicTrace(tracer(), "numberOfExplicitParameters = %d  pca.getNumPrevConstArgs = %d\n", explicitParams, pca.getNumPrevConstArgs(explicitParams));
               if (explicitParams > 0 && explicitParams <= pca.getNumPrevConstArgs(explicitParams))
                  allconsts = true;

               TR_CallSite *callsite = NULL;
               if (isInterface)
                  {
                  TR_OpaqueClassBlock * thisClass = NULL;
                  callsite = new (comp()->trHeapMemory()) TR_J9InterfaceCallSite(
                     callStack._method, callNodeTreeTop, parent, callNode,
                     interfaceMethod, thisClass, -1, cpIndex, resolvedMethod,
                     resolvedSymbol, isIndirectCall, isInterface, newBCInfo,
                     comp(), _recursionDepth, allconsts);
                  }
               else if (isIndirectCall)
                  {
                  callsite = new (comp()->trHeapMemory()) TR_J9VirtualCallSite(
                     callStack._method, callNodeTreeTop, parent, callNode,
                     interfaceMethod, resolvedMethod->classOfMethod(), -1, cpIndex,
                     resolvedMethod, resolvedSymbol, isIndirectCall, isInterface,
                     newBCInfo, comp(), _recursionDepth, allconsts);
                  }
               else
                  {
                  callsite = new (comp()->trHeapMemory()) TR_DirectCallSite(
                     callStack._method, callNodeTreeTop, parent, callNode,
                     interfaceMethod, resolvedMethod->classOfMethod(), -1, cpIndex,
                     resolvedMethod, resolvedSymbol, isIndirectCall, isInterface,
                     newBCInfo, comp(), _recursionDepth, allconsts);
                  }

              if(tracer()->debugLevel())
                 {
                 pca.printIndexes(comp());
                 }
              if(isInterface && pca.isArgAtIndexReceiverObject(interfaceMethod->numberOfExplicitParameters()))
                  {
                  //heuristicTrace(tracer(),"Arg at index %d is receiver object.  Propagating prexarginfo",interfaceMethod->numberOfExplicitParameters());
                  //callsite->_ecsPrexArgInfo = calltarget->_ecsPrexArgInfo;
                  }


                  TR_PrexArgInfo *argInfo = calltarget->_ecsPrexArgInfo;

                  if (wasPeekingSuccessfull)
                     {
                     TR_PrexArgInfo::propagateReceiverInfoIfAvailable(methodSymbol, callsite, argInfo, tracer());
                     }

               callsite->_callerBlock = currentInlinedBlock;

               if (isInlineable(&callStack, callsite))
                  {

                  if (wasPeekingSuccessfull)
                     {
                     TR_PrexArgInfo::propagateArgsFromCaller(methodSymbol, callsite, argInfo, tracer());
                     }

                  callSites[i] = callsite;
                  inlineableCallExists = true;
                  if (!currentInlinedBlock->isCold())
                     nonColdCallExists = true;
                  }
               else
                  calltarget->addDeadCallee(callsite);
               }
               flags[i].set(isUnsanitizeable);
               break;
            default:
               break;
            }

           pca.updateArg(bc );

           if (callSites[i])
              {
              for (int kk = 0; kk < callSites[i]->numTargets(); kk++)
                 callSites[i]->getTarget(kk)->_originatingBlock = currentInlinedBlock;
              }
           }
      _hasNonColdCalls = nonColdCallExists;

      if (comp()->isServerInlining())
         {
         int coldCode = 0;
         int executedCode = 0;
         bool isCold = false;
         int coldBorderFrequency = 20;

         for (i = 0; i < (int32_t) maxIndex; ++i)
            {
            if (blocks[i])
               if (!blocks[i]->isCold() && blocks[i]->getFrequency() > coldBorderFrequency)
                  isCold = false;
               else
                  isCold = true;

            if (isCold)
               coldCode++;
            else
               executedCode++;
            }

         if (executedCode != 0)
            {
            float ratio = ((float) executedCode) / ((float) (coldCode
                  + executedCode));

            if (recurseDown)
               {
               if (ratio < 0.7f)
                  {
                  ratio = 0.7f;
                  }
               }
            else
               {
               if (ratio < 0.1f)
                  {
                  ratio = 0.1f;
                  }
               }

            calltarget->_fullSize = (int) ((float) calltarget->_fullSize * ratio);
            heuristicTrace(tracer(),"Depth %d: Opt Server is reducing size of call to %d",_recursionDepth,calltarget->_fullSize);
            }
         }
      else if (_inliner->getPolicy()->aggressiveSmallAppOpts())
         {
         TR_J9InlinerPolicy *j9inlinerPolicy = (TR_J9InlinerPolicy *) _inliner->getPolicy();
         if (j9inlinerPolicy->aggressivelyInlineInLoops() && calltarget && calltarget->_calleeMethod && strncmp(calltarget->_calleeMethod->classNameChars(),"java/math/BigDecimal",calltarget->_calleeMethod->classNameLength())!=0)
            {
            if ((callStack._inALoop) &&
                (calltarget->_fullSize > 10))
                {
                calltarget->_fullSize = 10;
                heuristicTrace(tracer(),"Opt Server is reducing size of call to %d",calltarget->_fullSize);
                }
            }
        else
          heuristicTrace(tracer(),"Omitting Big Decimal method from size readjustment, calltarget = %p calleemethod = %p",calltarget,calltarget ? calltarget->_calleeMethod : 0);
         }

      /*************** PHASE 3:  Optimistically Assume we can partially inline calltarget and add to an optimisticSize ******************/

      calltarget->_cfg = &cfg;

      bool isCandidate = trimBlocksForPartialInlining(calltarget, &callBlocks);

      switch (calltarget->_calleeMethod->getRecognizedMethod())
         {
         case TR::java_util_HashMap_get:
         case TR::java_util_HashMap_findNonNullKeyEntry:
         calltarget->_isPartialInliningCandidate = false;
         isCandidate = false;
            break;
         default:
         	break;
         }

      if (isCandidate)
         _optimisticSize += calltarget->_partialSize;
      else
         _optimisticSize += calltarget->_fullSize;

      int32_t sizeThreshold = _sizeThreshold;
      if (isCandidate)
         sizeThreshold = std::max(4096, sizeThreshold);
      ///if(_optimisticSize > _sizeThreshold)   // even optimistically we've blown our budget
      heuristicTrace(tracer(),"--- Depth %d: Checking Optimistic size vs Size Threshold: _optimisticSize %d _sizeThreshold %d sizeThreshold %d ",_recursionDepth, _optimisticSize, _sizeThreshold, sizeThreshold);

      if (_optimisticSize > sizeThreshold) // even optimistically we've blown our budget
         {
         calltarget->_isPartialInliningCandidate = false;
         heuristicTrace(tracer(), "*** Depth %d: ECS end for target %p signature %s. optimisticSize exceeds Size Threshold", _recursionDepth, calltarget, callerName);
         return returnCleanup(2);
         }

      if (!recurseDown)
         {
         heuristicTrace(tracer(),"*** Depth %d: ECS end for target %p signature %s. recurseDown set to false. size = %d _fullSize = %d", _recursionDepth, calltarget, callerName, size, calltarget->_fullSize);
         return returnCleanup(0);
         }

      /****************** Phase 4: Deal with Inlineable Calls **************************/

      for (i = 0; i < maxIndex && inlineableCallExists; ++i)
         {
         //heuristicTrace(tracer(),"--- Depth %d: Checking _real size vs Size Threshold: _realSize %d _sizeThreshold %d sizeThreshold %d ",_recursionDepth, _realSize, _sizeThreshold, sizeThreshold);

         if (_realSize > sizeThreshold)
            {
            heuristicTrace(tracer(),"*** Depth %d: ECS end for target %p signature %s. real size %d exceeds sizeThreshold %d", _recursionDepth,calltarget, callerName,_realSize,sizeThreshold);
            return returnCleanup(4);
            }

         if (blocks[i])
            currentBlock = blocks[i];

         bci.setIndex(i);
         newBCInfo.setByteCodeIndex(i);
         if (callSites[i])
            {
            callSites[i]->setDepth(_recursionDepth);
            debugTrace(tracer(),"Found a call at bytecode %d, depth = %d", i, _recursionDepth);

            // TODO: Investigate if we should add BigAppOpts opts here


            for (int32_t j = 0; j < callSites[i]->numTargets(); j++)
               {
               TR_CallTarget *targetCallee = callSites[i]->getTarget(j);

               char nameBuffer[1024];
               const char *calleeName = NULL;
               if (tracer()->heuristicLevel())
                  calleeName = comp()->fej9()->sampleSignature(targetCallee->_calleeMethod->getPersistentIdentifier(), nameBuffer, 1024, comp()->trMemory());

               if (callGraphEnabled && !currentBlock->isCold())
                  {
                  // if call-graph profiling is enabled and the call is special or static (!indirect)
                  // then update the block frequency information because we don't profile predictable calls
                  if (!callSites[i]->isIndirectCall())
                     {
                     profileManager->updateCallGraphProfilingCount( currentBlock, calltarget->_calleeMethod->getPersistentIdentifier(), i, comp());
                     heuristicTrace(tracer(),"Depth %d: Updating Call Graph Profiling Count for calltarget %p count = %d",_recursionDepth, calltarget,profileManager->getCallGraphProfilingCount(calltarget->_calleeMethod->getPersistentIdentifier(), i, comp()));
                     }

                  // TODO: This coldCallInfoIsReliable logic should be in a more
                  // central place so everyone agrees on it.  It shouldn't just be
                  // for inliner.
                  //
                  bool coldCallInfoIsReliable = !cameFromArchetypeSpecimen( currentBlock->getJ9EstimateCodeSizeMethod());

                  if (_inliner->getPolicy()->tryToInline(targetCallee, &callStack, true))
                     {
                     heuristicTrace(tracer(),"tryToInline filter matched %s", targetCallee->_calleeMethod->signature(comp()->trMemory()));
                     }
                  else
                     {
                     int32_t freqCutoff = 40;
                     bool isColdCall = (((comp()->getMethodHotness() <= warm) && profileManager->isColdCall(targetCallee->_calleeMethod->getPersistentIdentifier(), calltarget->_calleeMethod->getPersistentIdentifier(), i, comp())) || (currentBlock->getFrequency() < freqCutoff)) && !_inliner->alwaysWorthInlining(targetCallee->_calleeMethod, NULL);

                     if (coldCallInfoIsReliable && isColdCall)
                        {
                        heuristicTrace(tracer(),"Depth %d: Skipping estimate on call %s, with count=%d and block frequency %d, because it's cold.",_recursionDepth,calleeName,profileManager->getCallGraphProfilingCount(targetCallee->_calleeMethod->getPersistentIdentifier(), calltarget->_calleeMethod->getPersistentIdentifier(), i, comp()), currentBlock->getFrequency());
                        callSites[i]->removecalltarget(j, tracer(), Cold_Call);
                        j--;
                        continue;
                        }

                     if (comp()->getMethodHotness() <= warm && comp()->isServerInlining() && calltarget->_calleeMethod->isWarmCallGraphTooBig(i, comp()) && !_inliner->alwaysWorthInlining(targetCallee->_calleeMethod, NULL))
                        {
                        heuristicTrace(tracer(), "Depth %d: Skipping estimate on call %s, with count=%d, because its warm call graph is too big.",
                                               _recursionDepth, calleeName,
                                               profileManager->getCallGraphProfilingCount(calltarget->_calleeMethod->getPersistentIdentifier(),i, comp())
                                             );
                        callSites[i]->removecalltarget(j, tracer(), Cold_Call);
                        j--;
                        continue;
                        }
                     }
                  }

               //inline Native method even if it is cold as the Natives
               //are usually very small and inlining them would not hurt
               if (currentBlock->isCold() && !_inliner->alwaysWorthInlining(targetCallee->_calleeMethod, callSites[i]->_callNode))
                  {
                  heuristicTrace(tracer(),"Depth %d: Skipping estimate on call %s, because it's in a cold block.",_recursionDepth, calleeName);
                  callSites[i]->removecalltarget(j, tracer(), Cold_Block);
                  j--;
                  continue;
                  }

               if (_optimisticSize <= sizeThreshold) // for multiple calltargets, is this the desired behaviour?
                  {
                  _recursionDepth++;
                  _numOfEstimatedCalls++;

                  _lastCallBlockFrequency = currentBlock->getFrequency();

                  debugTrace(tracer(),"About to call ecs on call target %p at depth %d _optimisticSize = %d _realSize = %d _sizeThreshold = %d",
                                       targetCallee, _recursionDepth, _optimisticSize, _realSize, _sizeThreshold);
                  heuristicTrace(tracer(),"--- Depth %d: EstimateCodeSize to recursively estimate call from %s to %s",_recursionDepth, callerName, calleeName);

                  int32_t origOptimisticSize = _optimisticSize;
                  int32_t origRealSize = _realSize;
                  bool prevNonColdCalls = _hasNonColdCalls;
                 // if(recurseDown)
                     bool estimateSuccess = estimateCodeSize(targetCallee, &callStack); //,recurseDown);
                  bool calltargetSetTooBig = false;
                  bool calleeHasNonColdCalls = _hasNonColdCalls;
                  _hasNonColdCalls = prevNonColdCalls;// reset the bool for the parent

                  // update optimisticSize and cull candidates

                  if ((comp()->getMethodHotness() >= warm) && comp()->isServerInlining())
                     {
                     int32_t bigCalleeThreshold;
                     int32_t freqCutoff = comp()->getMethodHotness() <= warm ?
                                             comp()->getOptions()->getBigCalleeFrequencyCutoffAtWarm() :
                                             comp()->getOptions()->getBigCalleeFrequencyCutoffAtHot();
                     bool isColdCall = ((profileManager->isColdCall(targetCallee->_calleeMethod->getPersistentIdentifier(), calltarget->_calleeMethod->getPersistentIdentifier(), i, comp()) ||
                           (currentBlock->getFrequency() <= freqCutoff)) && !_inliner->alwaysWorthInlining(targetCallee->_calleeMethod, NULL));

                     if (comp()->getMethodHotness() <= warm)
                        {
                        bigCalleeThreshold = isColdCall ?
                                                comp()->getOptions()->getBigCalleeThresholdForColdCallsAtWarm():
                                                comp()->getOptions()->getBigCalleeThreshold();
                        }
                     else // above warm
                        {

                        if(isColdCall)
                           {
                           bigCalleeThreshold = comp()->getOptions()->getBigCalleeThresholdForColdCallsAtHot();
                           }
                        else
                           {
                           if (comp()->getMethodHotness() == scorching ||
                              (comp()->getMethodHotness() == veryHot && comp()->isProfilingCompilation()))
                              {
                              bigCalleeThreshold = comp()->getOptions()->getBigCalleeScorchingOptThreshold();
                              }
                           else
                              {
                              bigCalleeThreshold = comp()->getOptions()->getBigCalleeHotOptThreshold();
                              }
                           }
                        }


                     if (_optimisticSize - origOptimisticSize > bigCalleeThreshold)
                        {
                        ///printf("set warmcallgraphtoobig for method %s at index %d\n", calleeName, newBCInfo._byteCodeIndex);fflush(stdout);
                        calltarget->_calleeMethod->setWarmCallGraphTooBig( newBCInfo.getByteCodeIndex(), comp());
                        traceMsg(comp(), "set warmcallgraphtoobig for method %s at index %d\n", calleeName, newBCInfo.getByteCodeIndex());
                        //_optimisticSize = origOptimisticSize;
                        //_realSize = origRealSize;
                        calltargetSetTooBig = true;
                        }
                     }

                  if (!estimateSuccess && !calltargetSetTooBig)
                     {
                     int32_t estimatedSize = (_optimisticSize - origOptimisticSize);
                     int32_t bytecodeSize = targetCallee->_calleeMethod->maxBytecodeIndex();
                     bool inlineAnyway = false;

                     if ((_optimisticSize - origOptimisticSize) < 40)
                        inlineAnyway = true;
                     else if (estimatedSize < 100)
                        {
                        if ((estimatedSize < bytecodeSize) || ((bytecodeSize - estimatedSize)< 20))
                           inlineAnyway = true;
                        }

                     if (inlineAnyway && !calleeHasNonColdCalls)
                        {
                        _optimisticSize = origOptimisticSize;
                        _realSize = origRealSize;
                        }
                     else if (!_inliner->alwaysWorthInlining(targetCallee->_calleeMethod, NULL))
                        {
                        calltarget->_isPartialInliningCandidate = false;
                        callSites[i]->removecalltarget(j, tracer(),
                              Callee_Too_Many_Bytecodes);
                        _optimisticSize = origOptimisticSize;
                        _realSize = origRealSize;
                        calltarget->addDeadCallee(callSites[i]);
                        j--;
                        _numOfEstimatedCalls--;
                        }

                     if(comp()->getVisitCount() > HIGH_VISIT_COUNT)
                        {
                        heuristicTrace(tracer(),"Depth %d: estimateCodeSize aborting due to high comp()->getVisitCount() of %d",_recursionDepth,comp()->getVisitCount());
                        return returnCleanup(3);
                        }
                     }
                  else if (calltargetSetTooBig)
                     {
                     _optimisticSize = origOptimisticSize;
                     _realSize = origRealSize;

                     if (!_inliner->alwaysWorthInlining(targetCallee->_calleeMethod, NULL))
                        {
                        calltarget->_isPartialInliningCandidate = false;
                        callSites[i]->removecalltarget(j, tracer(),
                              Callee_Too_Many_Bytecodes);
                        calltarget->addDeadCallee(callSites[i]);
                        j--;
                        _numOfEstimatedCalls--;
                        }

                     if(comp()->getVisitCount() > HIGH_VISIT_COUNT)
                        {
                        heuristicTrace(tracer(),"Depth %d: estimateCodeSize aborting due to high comp()->getVisitCount() of %d",_recursionDepth,comp()->getVisitCount());
                        return returnCleanup(3);
                        }
                     }
                  else
                     {
                     }

                  _recursionDepth--;
                  }
               else
                  {
                  heuristicTrace(tracer(),"Depth %d: estimateCodeSize aborting due to _optimisticSize: %d > sizeThreshold: %d",_optimisticSize,sizeThreshold);
                  break;
                  }
               }

            if (callSites[i]->numTargets()) //only add a callSite once, even though it may have more than one call target.
               {
               calltarget->addCallee(callSites[i]);
               heuristicTrace(tracer(), "Depth %d: Subtracting %d from optimistic and real size to account for eliminating call", _recursionDepth, bci.estimatedCodeSize());
               if (_optimisticSize > bci.estimatedCodeSize())
                  _optimisticSize -= bci.estimatedCodeSize(); // subtract what we added before for the size of the call instruction
               if (_realSize > bci.estimatedCodeSize())
                  _realSize -= bci.estimatedCodeSize();
               }
            }
         }

	auto partialSizeBeforeAdjustment = calltarget->_partialSize;
	
   if (adjustEstimateForStringCompression(calltarget->_calleeMethod, calltarget->_partialSize, STRING_COMPRESSION_ADJUSTMENT_FACTOR))
      {
      heuristicTrace(tracer(), "*** Depth %d: Adjusting partial size for %s because of string compression from %d to %d", _recursionDepth, callerName, partialSizeBeforeAdjustment, calltarget->_partialSize);
      }
   
   auto fullSizeBeforeAdjustment = calltarget->_fullSize;
   
   if (adjustEstimateForStringCompression(calltarget->_calleeMethod, calltarget->_fullSize, STRING_COMPRESSION_ADJUSTMENT_FACTOR))
      {
      heuristicTrace(tracer(), "*** Depth %d: Adjusting full size for %s because of string compression from %d to %d", _recursionDepth, callerName, fullSizeBeforeAdjustment, calltarget->_fullSize);
      }
      
   auto realSizeBeforeAdjustment = _realSize;
   
   if (adjustEstimateForStringCompression(calltarget->_calleeMethod, _realSize, STRING_COMPRESSION_ADJUSTMENT_FACTOR))
      {
      heuristicTrace(tracer(), "*** Depth %d: Adjusting real size for %s because of string compression from %d to %d", _recursionDepth, callerName, realSizeBeforeAdjustment, _realSize);
      }
   
      reduceDAAWrapperCodeSize(calltarget);

      /****************** PHASE 5: Figure out if We're really going to do a partial Inline and add whatever we do to the realSize. *******************/

      if (isPartialInliningCandidate(calltarget, &callBlocks))
         {
         if (comp()->getOption(TR_TraceBFGeneration))
            traceMsg(comp(), "Call Target %s is a partial inline Candidate with a partial size of %d",callerName,calltarget->_partialSize);

         heuristicTrace(tracer(), "*** Depth %d: ECS end for target %p signature %s. It is a partial inline Candidate with a partial size of %d", _recursionDepth, calltarget, callerName, calltarget->_partialSize);
         _realSize += calltarget->_partialSize;
         }
      else
         {
         heuristicTrace(tracer(),"*** Depth %d: ECS end for target %p signature %s. It is a full inline Candidate with a full size of %d", _recursionDepth, calltarget, callerName, calltarget->_fullSize);
         _realSize += calltarget->_fullSize;
         }


      heuristicTrace(tracer(),"--- Depth %d: Checking _real size vs Size Threshold A second Time: _realSize %d _sizeThreshold %d sizeThreshold %d ",_recursionDepth, _realSize, _sizeThreshold, sizeThreshold);

      if (_realSize > sizeThreshold)
         {
         heuristicTrace(tracer(),"*** Depth %d: ECS end for target %p signature %s. real size exceeds Size Threshold", _recursionDepth,calltarget, callerName);
         return returnCleanup(4);
         }
      }

   return returnCleanup(0);
   }

bool TR_J9EstimateCodeSize::reduceDAAWrapperCodeSize(TR_CallTarget* target)
   {
   if (target == NULL)
      return false;

   // DAA Wrappers are basically free if intrinsics are on since all they consist of is the slow and fast paths
   if (target->_calleeMethod)
      {
      bool reduceMarshallingWrapper = target->_calleeMethod->isDAAMarshallingWrapperMethod() &&
              !comp()->getOption(TR_DisableMarshallingIntrinsics);

      bool reducePackedDecimalWrapper = target->_calleeMethod->isDAAPackedDecimalWrapperMethod() &&
              !comp()->getOption(TR_DisableMarshallingIntrinsics);

      if (reduceMarshallingWrapper || reducePackedDecimalWrapper)
         {
         target->_fullSize    /= 5;
         target->_partialSize /= 5;

         heuristicTrace(tracer(),"DAA: Reducing target %p fullSize to %d and partialSize to %d to increase likelyhood of successful inlining\n", target, target->_fullSize, target->_partialSize);
         return true;
         }
      }

   return false;
   }

/******************
 * A graph searching algorithm.  searchItem is the flag type we're looking for, searchPath is the flag type of the path we can go down
 *
 * ***************/

bool
TR_J9EstimateCodeSize::graphSearch(TR::CFG *cfg, TR::Block *startBlock,
      TR::Block::partialFlags searchItem, TR::Block::partialFlags searchPath)
   {
   TR_BitVector *blocksVisited = new (comp()->trStackMemory()) TR_BitVector(
         cfg->getNextNodeNumber(), comp()->trMemory(), stackAlloc);
   blocksVisited->empty();

   TR_Queue<TR::Block> nodesToBeEvaluated(comp()->trMemory());
   nodesToBeEvaluated.enqueue(startBlock);

   do
      {
      TR::Block *currentBlock = nodesToBeEvaluated.dequeue();

      if (blocksVisited->get(currentBlock->getNumber()))
         continue;
      blocksVisited->set(currentBlock->getNumber());

      if (currentBlock->getPartialFlags().testAny(searchItem))
         return true;

      for (auto e = currentBlock->getSuccessors().begin(); e != currentBlock->getSuccessors().end(); ++e)
         {
         TR::Block *dest = (*e)->getTo()->asBlock();
         if (dest->getPartialFlags().testAny(searchPath))
            nodesToBeEvaluated.enqueue(dest);
         }
      for (auto e = currentBlock->getExceptionSuccessors().begin(); e != currentBlock->getExceptionSuccessors().end(); ++e)
         {
         TR::Block *dest = (*e)->getTo()->asBlock();
         if (dest->getPartialFlags().testAny(searchPath))
            nodesToBeEvaluated.enqueue(dest);
         }
      }
   while (!nodesToBeEvaluated.isEmpty());

   return false; //did not find the search item
   }

/*************************
 * A graph labelling algorithm
 *      TODO: you can add size information in here
 * ***********************/
#define MIN_PARTIAL_FREQUENCY 15
int32_t
TR_J9EstimateCodeSize::labelGraph(TR::CFG *cfg,
      TR_Queue<TR::Block> *unsanitizeableBlocks, TR_Queue<TR::Block> *callBlocks)
   {
   TR_BitVector *blocksVisited = new (comp()->trStackMemory()) TR_BitVector(
         cfg->getNextNodeNumber(), comp()->trMemory(), stackAlloc);
   blocksVisited->empty();

   int32_t size = 0;
   bool hasAtLeastOneRestartBlock = false;
   TR::Block *startBlock = cfg->getStart()->asBlock();
   TR::Block *endBlock = cfg->getEnd()->asBlock();
   TR_Queue<TR::Block> nodesToBeEvaluated(comp()->trMemory());
   TR_Queue<TR::Block> difficultNodesToBeEvaluated(comp()->trMemory());
   nodesToBeEvaluated.enqueue(endBlock);

   TR::Block *currentBlock = NULL;

   do
      {
      if (!nodesToBeEvaluated.isEmpty())
         currentBlock = nodesToBeEvaluated.dequeue();
      else if (!difficultNodesToBeEvaluated.isEmpty())
         currentBlock = difficultNodesToBeEvaluated.dequeue();
      else
         TR_ASSERT(0, "Neither Queue has a node left!\n");

      if (blocksVisited->get(currentBlock->getNumber()))
         continue;
      //      blocksVisited->set(currentBlock->getNumber()); // moving this downward a little!

      if (currentBlock->getBlockSize() == -1 && (currentBlock != startBlock
            && currentBlock != endBlock))
         TR_ASSERT(0, "labelGraph:  a block does not have a valid size!\n");

      //Part 1:  Successor Test:  ensure all my successors have been evaluated first and that they are not all restart blocks.

      bool allRestarts = true;
      bool allVisited = true;
      for (auto e = currentBlock->getSuccessors().begin(); e != currentBlock->getSuccessors().end(); ++e)
         {
         TR::Block *dest = (*e)->getTo()->asBlock();

         if (!blocksVisited->get(dest->getNumber()))
            {
            allVisited = false;
            break;
            }

         if (!dest->isRestartBlock())
            {
            allRestarts = false;
            break;
            }
         }
      for (auto e = currentBlock->getExceptionSuccessors().begin(); e != currentBlock->getExceptionSuccessors().end(); ++e)
         {
         TR::Block *dest = (*e)->getTo()->asBlock();

         if (!blocksVisited->get(dest->getNumber()))
            {
            allVisited = false;
            //            break;
            }

         if (dest->isPartialInlineBlock()) //(!dest->isRestartBlock())
            {
            //            allRestarts=false;
            //            break;
            }
         }

      if (!allVisited && !currentBlock->isDifficultBlock())
         {

         partialTrace(tracer(), "Requeueing block into difficult Nodes List %p %d because its successors have not been all visited \n", currentBlock, currentBlock->getNumber());
         currentBlock->setIsDifficultBlock();
         difficultNodesToBeEvaluated.enqueue(currentBlock);
         continue;
         }
      else if (currentBlock->isDifficultBlock())
         {
         //assuming all unvisited blocks are restarts.
         //which actually means doing nothing here, since I only mark allRestarts = false if I found a  partial inline block.

         blocksVisited->set(currentBlock->getNumber());

         }
      else
         blocksVisited->set(currentBlock->getNumber());

      //Part 2: Setting Flags on the Current Block
      int16_t minpartialfreq = MIN_PARTIAL_FREQUENCY;


      if (allRestarts && currentBlock != cfg->getEnd()->asBlock())
         {
         currentBlock->setRestartBlock();
         hasAtLeastOneRestartBlock = true;
         if (currentBlock->isPartialInlineBlock())
            {
            currentBlock->setPartialInlineBlock(false);
            if (currentBlock != startBlock && currentBlock != endBlock)
               {
               if (size > currentBlock->getBlockSize())
                  size -= currentBlock->getBlockSize();
               }
            }
         }
      else if ((currentBlock->getFrequency() < minpartialfreq  || currentBlock->isCold()) && currentBlock != startBlock && currentBlock != endBlock)
         {
         currentBlock->setRestartBlock();
         hasAtLeastOneRestartBlock = true;
         }
      else
         {
         currentBlock->setPartialInlineBlock();
         if (currentBlock != startBlock && currentBlock != endBlock)
            size += currentBlock->getBlockSize();
         }

      if (currentBlock->isUnsanitizeable())
         unsanitizeableBlocks->enqueue(currentBlock);
      else if (currentBlock->containsCall()) //only need to enqueue it if its not unsanitizeable already
         callBlocks->enqueue(currentBlock);

      // Part 3:  Enqueue all Predacessors

      for (auto e = currentBlock->getPredecessors().begin(); e != currentBlock->getPredecessors().end(); ++e)
         {
         TR::Block *dest = (*e)->getFrom()->asBlock();
         nodesToBeEvaluated.enqueue(dest);
         }
      for (auto e = currentBlock->getExceptionPredecessors().begin(); e != currentBlock->getExceptionPredecessors().end();
    		  ++e)
         {
         TR::Block *dest = (*e)->getFrom()->asBlock();
         nodesToBeEvaluated.enqueue(dest);
         }

      if (currentBlock->isRestartBlock()
            && currentBlock->isPartialInlineBlock())
         TR_ASSERT(0, "currentBlock is both a restart block AND a partial inline block!\n");

      }
   while (!nodesToBeEvaluated.isEmpty()
         || !difficultNodesToBeEvaluated.isEmpty());

   if (!hasAtLeastOneRestartBlock)
      return -1; // this means I should just do a full inline anyways
   return size;
   }
#define MIN_PARTIAL_SIZE 100

bool
TR_J9EstimateCodeSize::trimBlocksForPartialInlining(TR_CallTarget *calltarget, TR_Queue<TR::Block> *callBlocks)
   {
   TR_ASSERT(calltarget->_originatingBlock, "trimBlocksForPartialInlining: call target does not have an _originatingBlock set yet!\n");

   if (comp()->getOption(TR_DisablePartialInlining) || calltarget->_calleeMethod->isSynchronized())
      {
      calltarget->_isPartialInliningCandidate = false;
      return false;
      }

   TR_Queue<TR::Block> unsanitizeableBlocks(comp()->trMemory());

   int32_t size = labelGraph(calltarget->_cfg, &unsanitizeableBlocks,
         callBlocks);

   if (tracer()->partialLevel())
      {
      partialTrace(tracer(),"Dumping CFG for calltarget %p", calltarget);
      comp()->dumpFlowGraph(calltarget->_cfg);
      }

   int32_t minpartialsize = MIN_PARTIAL_SIZE;

   if (size > -1 && size + minpartialsize >= calltarget->_fullSize)
      {
      partialTrace(tracer()," Candidate partial size of %d is too close to full Size of %d to be of any benefit.  Doing a full inline.",size, calltarget->_fullSize);
      }
   else if (size > -1) // a size of -1 means we didn't have any restart blocks - so no sense in doing a 'partial' inline
      {
      bool gs = true;
      while (!unsanitizeableBlocks.isEmpty())
         {
         TR::Block *aBlock = unsanitizeableBlocks.dequeue();
         if (!aBlock->isRestartBlock()) // if the unsanitizeable block is also a restart block, I don't care who it reaches.
            {
            calltarget->_originatingBlock->setIsUnsanitizeable(); // An unsanitizeable block remains in the inline

            gs = !(graphSearch(calltarget->_cfg, aBlock,
                  TR::Block::_restartBlock,
                  (TR::Block::partialFlags) (TR::Block::_partialInlineBlock
                        | TR::Block::_restartBlock)));
            if (!gs)
               {
               partialTrace(tracer(),"TrimBlocksForPartialInlining: Unsanitizeable block %p %d can reach a restart block.",aBlock, aBlock->getNumber());
               break;
               }
            }
         else
            partialTrace(tracer(),"TrimBlocksForPartialinlining: Unsanitizeable block %p %d is a restart block.",aBlock, aBlock->getNumber());
         }

      if (gs)
         {
         gs = graphSearch(calltarget->_cfg,
               calltarget->_cfg->getStart()->asBlock(), TR::Block::_endBlock,
               TR::Block::_partialInlineBlock);
         if (!gs)
            partialTrace(tracer(),"TrimBlocksForPartialInlining: No Complete Path from Start to End");
         }

      if (!gs)
         {
         calltarget->_isPartialInliningCandidate = false;
         return false;
         }

      partialTrace(tracer(), "TrimBlocksForPartialInlining Found a Candidate.  Setting PartialSize to %d. full size = %d",size, calltarget->_fullSize);
      calltarget->_partialSize = size;

      return true;
      }
   else
      {
      if (!unsanitizeableBlocks.isEmpty())
         calltarget->_originatingBlock->setIsUnsanitizeable(); // A Full Inline with unsanitizeable blocks
      partialTrace(tracer(),"TrimBlocksForPartialInlining: No restart blocks found in candidate. Doing a full inline");
      }

   calltarget->_isPartialInliningCandidate = false;
   return false;
   }

void
TR_J9EstimateCodeSize::processGraph(TR_CallTarget *calltarget)
   {
   TR::CFG *cfg = calltarget->_cfg;
   calltarget->_partialInline = new (comp()->trHeapMemory()) TR_InlineBlocks(
         _inliner->fe(), _inliner->comp());
   TR_BitVector *blocksVisited = new (comp()->trStackMemory()) TR_BitVector(
         cfg->getNextNodeNumber(), comp()->trMemory(), stackAlloc);
   blocksVisited->empty();

   TR::Block *startBlock = cfg->getStart()->asBlock();
   TR::Block *endBlock = cfg->getEnd()->asBlock();
   TR_Queue<TR::Block> nodesToBeEvaluated(comp()->trMemory());
   nodesToBeEvaluated.enqueue(startBlock);

   do
      {
      TR::Block *currentBlock = nodesToBeEvaluated.dequeue();

      if (blocksVisited->get(currentBlock->getNumber()))
         continue;
      blocksVisited->set(currentBlock->getNumber());

      if (currentBlock != startBlock && currentBlock != endBlock)
         calltarget->_partialInline->addBlock(currentBlock);

      for (auto e = currentBlock->getSuccessors().begin(); e != currentBlock->getSuccessors().end(); ++e)
         {
         TR::Block *dest = (*e)->getTo()->asBlock();
         if (dest->isPartialInlineBlock())
            nodesToBeEvaluated.enqueue(dest);
         }
      for (auto e = currentBlock->getExceptionSuccessors().begin(); e != currentBlock->getExceptionSuccessors().end(); ++e)
         {
         TR::Block *dest = (*e)->getTo()->asBlock();
         if (dest->isPartialInlineBlock())
            nodesToBeEvaluated.enqueue(dest);

         calltarget->_partialInline->addExceptionBlock(dest); //only partial blocks will be processed.  any exception block reachable from a partial block needs to be dealt with.
         }

      }
   while (!nodesToBeEvaluated.isEmpty());

   }

/***************************************
 * isPartialInliningCandidate()
 * Checks any call blocks as being unsanitizeable and if they can reach a restart
 * Generates the list of TR_InlineBlocks that are to be inlined.
 * ***************************************/

bool
TR_J9EstimateCodeSize::isPartialInliningCandidate(TR_CallTarget *calltarget,
      TR_Queue<TR::Block> *callBlocks)
   {
   if (!calltarget->_isPartialInliningCandidate)
      return false;

   while (!callBlocks->isEmpty())
      {
      TR::Block *callBlock = callBlocks->dequeue();

      if (callBlock->isUnsanitizeable() && !callBlock->isRestartBlock())
         {
         calltarget->_originatingBlock->setIsUnsanitizeable();
         bool result = graphSearch(calltarget->_cfg, callBlock,
               TR::Block::_restartBlock,
               (TR::Block::partialFlags) (TR::Block::_partialInlineBlock
                     | TR::Block::_restartBlock));
         if (result) // unsanitizeable block can reach a restart block
            {
            calltarget->_isPartialInliningCandidate = false;
            return false;
            }
         }

      }

   // we have a partial inlining candidate at this point.  Now walk the graph and all P blocks to TR_InlineBlocks

   processGraph(calltarget);

   return true;
   }

