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

#ifndef TR_J9_CODEGENERATORBASE_INCL
#define TR_J9_CODEGENERATORBASE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef TRJ9_CODEGENERATORBASE_CONNECTOR
#define TRJ9_CODEGENERATORBASE_CONNECTOR

namespace J9 { class CodeGenerator; }
namespace J9 { typedef J9::CodeGenerator CodeGeneratorConnector; }
#endif

#include "codegen/OMRCodeGenerator.hpp"

#include <stdint.h>        // for int32_t
#include "env/IO.hpp"      // for POINTER_PRINTF_FORMAT
#include "env/jittypes.h"  // for uintptr_t
#include "infra/List.hpp"  // for List, etc
#include "codegen/RecognizedMethods.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "optimizer/Dominators.hpp"
#include "cs2/arrayof.h"   // for ArrayOf

class NVVMIRBuffer;
class TR_BitVector;
class TR_SharedMemoryAnnotations;
class TR_J9VMBase;
namespace TR { class Block; }
namespace TR { class Node; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }

namespace J9
{

class OMR_EXTENSIBLE CodeGenerator : public OMR::CodeGeneratorConnector
   {
public:

   CodeGenerator();

   TR_J9VMBase *fej9();

   TR::TreeTop *lowerTree(TR::Node *root, TR::TreeTop *treeTop);

   void preLowerTrees();

   void lowerTreesPreTreeTopVisit(TR::TreeTop *tt, vcount_t visitCount);

   void lowerTreesPreChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount);

   void lowerTreeIfNeeded(TR::Node *node, int32_t childNumber, TR::Node *parent, TR::TreeTop *tt);

   void lowerDualOperator(TR::Node *parent, int32_t childNumber, TR::TreeTop *treeTop);

   bool collectSymRefs(TR::Node *node, TR_BitVector *symRefs, vcount_t secondVisitCount);

   void moveUpArrayLengthStores(TR::TreeTop *insertionPoint);

   void doInstructionSelection();

   // OSR, not code generator
   void populateOSRBuffer();

   void lowerCompressedRefs(TR::TreeTop *, TR::Node*, vcount_t, TR_BitVector *);
   void compressedReferenceRematerialization(); // J9
   void rematerializeCompressedRefs(TR::SymbolReference * &, TR::TreeTop *, TR::Node*, int32_t, TR::Node*, vcount_t, List<TR::Node> *);
   void anchorRematNodesIfNeeded(TR::Node*, TR::TreeTop *, List<TR::Node> *);
   void yankCompressedRefs(TR::TreeTop *, TR::Node *, int32_t, TR::Node *, vcount_t, vcount_t);

   void setUpForInstructionSelection();

   void insertEpilogueYieldPoints();

   void splitWarmAndColdBlocks(); // J9 & Z

   void detectEndOfVMThreadGlobalRegisterLiveRange(TR::Block *block); // J9 & X86?

   void allocateLinkageRegisters();

   TR::Linkage *createLinkageForCompilation();

   bool enableAESInHardwareTransformations() {return false;}

   bool isMethodInAtomicLongGroup(TR::RecognizedMethod rm);

   // OSR
   //
   TR::TreeTop* genSymRefStoreToArray(TR::Node* refNode, TR::Node* arrayAddressNode, TR::Node* firstOffset,
                                      TR::Node* symRefLoad, int32_t secondOffset,  TR::TreeTop* insertionPoint);

   // --------------------------------------
   // AOT Relocations
   //

   void processRelocations();

   //TR::ExternalRelocation
   void addProjectSpecializedRelocation(uint8_t *location,
                                          uint8_t *target,
                                          uint8_t *target2,  //pass in NULL when no target2
                                          TR_ExternalRelocationTargetKind kind,
                                          char *generatingFileName,
                                          uintptr_t generatingLineNumber,
                                          TR::Node *node);
   //TR::ExternalOrderedPair32BitRelocation
   void addProjectSpecializedPairRelocation(uint8_t *location1,
                                          uint8_t *location2,
                                          uint8_t *target,
                                          TR_ExternalRelocationTargetKind kind,
                                          char *generatingFileName,
                                          uintptr_t generatingLineNumber,
                                          TR::Node *node);
   //TR::BeforeBinaryEncodingExternalRelocation
   void addProjectSpecializedRelocation(TR::Instruction *instr,
                                          uint8_t *target,
                                          uint8_t *target2,   //pass in NULL when no target2
                                          TR_ExternalRelocationTargetKind kind,
                                          char *generatingFileName,
                                          uintptr_t generatingLineNumber,
                                          TR::Node *node);

   bool needClassAndMethodPointerRelocations();
   bool needRelocationsForStatics();

   // ----------------------------------------

   void jitAddUnresolvedAddressMaterializationToPatchOnClassRedefinition(void *firstInstruction);

   uint32_t getStackLimitOffset() {return _stackLimitOffsetInMetaData;}
   uint32_t setStackLimitOffset(uint32_t o) {return (_stackLimitOffsetInMetaData = o);}

   // e.g. one or more pdclean nodes are present
   bool hasSignCleans() { return _flags4.testAny(HasSignCleans);}
   void setHasSignCleans() { _flags4.set(HasSignCleans);}

   bool alwaysGeneratesAKnownCleanSign(TR::Node *node) { return false; } // no virt
   bool alwaysGeneratesAKnownPositiveCleanSign(TR::Node *node) { return false; } // no virt
   TR_RawBCDSignCode alwaysGeneratedSign(TR::Node *node) { return raw_bcd_sign_unknown; } // no virt

   void foldSignCleaningIntoStore();
   void swapChildrenIfNeeded(TR::Node *store, char *optDetails);

   TR::AutomaticSymbol *allocateVariableSizeSymbol(int32_t size);
   TR::SymbolReference *allocateVariableSizeSymRef(int32_t byteLength);
   void pendingFreeVariableSizeSymRef(TR::SymbolReference *sym);
   void freeVariableSizeSymRef(TR::SymbolReference *sym, bool freeAddressTakenSymbol=false);
   void freeAllVariableSizeSymRefs();

   TR::SymbolReference *getFreeVariableSizeSymRef(int byteLength);
   void checkForUnfreedVariableSizeSymRefs();

   void  registerAssumptions();

   void jitAddPicToPatchOnClassUnload(void *classPointer, void *addressToBePatched);
   void jitAdd32BitPicToPatchOnClassUnload(void *classPointer, void *addressToBePatched);
   void jitAddPicToPatchOnClassRedefinition(void *classPointer, void *addressToBePatched, bool unresolved = false);
   void jitAdd32BitPicToPatchOnClassRedefinition(void *classPointer, void *addressToBePatched, bool unresolved = false);

   void createHWPRecords();

   void createStackAtlas();

   // --------------------------------------------------------------------------
   // GPU
   //
   static const int32_t GPUAlignment = 128;
   uintptrj_t objectLengthOffset();
   uintptrj_t objectHeaderInvariant();

  enum GPUScopeType
      {
      naturalLoopScope = 0,
      singleKernelScope = 1
      };

   enum GPUResult
      {
      GPUSuccess = 0,
      GPUNullCheck = 1,
      GPUBndCheck = 2,
      GPUDivException = 3,
      GPUInvalidProgram = 4,
      GPUHelperError = 5,
      GPULaunchError = 6,
      GPUBadDevicePointer = 7
      };

   enum GPUAccessKind {None = 0, ReadAccess= 1, WriteAccess = 2, ReadWriteAccesses = 3};
   class gpuMapElement
      {
      public:
      TR_ALLOC(TR_Memory::CodeGenerator); // dummy

      gpuMapElement() : _node(NULL), _hostSymRef(NULL), _hostSymRefTemp(NULL), _devSymRef(NULL), _hoistAccess(false), _elementSize(-1), _parmSlot(-1), _accessKind(None), _lhsAddrExpr(NULL), _rhsAddrExpr(NULL) {}

      gpuMapElement(TR::Node *node, TR::SymbolReference *hostSymRef, int32_t elementSize, int32_t parmSlot)
                    :
                    _node(node), _hostSymRef(hostSymRef), _hostSymRefTemp(NULL), _devSymRef(NULL), _hoistAccess(false),
                    _elementSize(elementSize), _parmSlot(parmSlot), _accessKind(None), _lhsAddrExpr(NULL), _rhsAddrExpr(NULL) {}

      TR::Node          *_node;
      TR::SymbolReference *_hostSymRef;
      TR::SymbolReference *_hostSymRefTemp;
      TR::SymbolReference *_devSymRef;
      int32_t             _elementSize;
      int32_t             _parmSlot;
      uint32_t            _accessKind;
      bool                _hoistAccess;
      TR::Node          *_rhsAddrExpr;
      TR::Node          *_lhsAddrExpr;
      };

   class gpuParameter
      {
      public:
      TR_ALLOC(TR_Memory::CodeGenerator); // dummy

      gpuParameter() : _hostSymRef(NULL), _parmSlot(-1) {}

      gpuParameter(TR::SymbolReference *hostSymRef, int32_t parmSlot)
                    : _hostSymRef(hostSymRef), _parmSlot(parmSlot) {}

      TR::SymbolReference *_hostSymRef;
      int32_t             _parmSlot;
      };

   CS2::ArrayOf<gpuMapElement, TR::Allocator> _gpuSymbolMap;

   bool _gpuHasNullCheck;
   bool _gpuHasBndCheck;
   bool _gpuHasDivCheck;

   scount_t _gpuNodeCount;
   TR::Block *_gpuCurrentBlock;
   TR::DataTypes _gpuReturnType;
   TR_Dominators *_gpuPostDominators;
   TR::Block *_gpuStartBlock;
   uint64_t _gpuNeedNullCheckArguments_vector;
   bool _gpuCanUseReadOnlyCache;
   bool _gpuUseOldLdgCalls;


private:

   uint16_t changeParmLoadsToRegLoads(TR::Node*node, TR::Node **regLoads, TR_BitVector *globalRegsWithRegLoad, TR_BitVector &killedParms, vcount_t visitCount); // returns number of RegLoad nodes created

   static bool wantToPatchClassPointer(TR::Compilation *comp,
                                       const TR_OpaqueClassBlock *allegedClassPointer,
                                       const char *locationDescription,
                                       const void *location)
      {
      // If we have a class pointer to consider, it should look like one.
      const uintptrj_t j9classEyecatcher = 0x99669966;
      if (allegedClassPointer != NULL)
         {
         TR_ASSERT(*(const uintptrj_t*)allegedClassPointer == j9classEyecatcher,
                   "expected a J9Class* for omitted runtime assumption");
         }

      // Class pointer patching is restricted to HCR mode.
      if (!comp->getOption(TR_EnableHCR))
         return false;

      // -Xjit:HCRPatchClassPointers re-enables all class pointer assumptions
      if (comp->getOption(TR_HCRPatchClassPointers))
         return true;

      return !performTransformation(comp,
                                    "O^O OMIT HCR CLASS POINTER ASSUMPTION: class=" POINTER_PRINTF_FORMAT ", %s " POINTER_PRINTF_FORMAT "\n", allegedClassPointer,
                                    locationDescription,
                                    location);
      }

   uint32_t _stackLimitOffsetInMetaData;

   /*
    * Scratch data for refined aliasing walk during
    */
   struct RefinedAliasWalkCollector
      {
      TR_PersistentMethodInfo *methodInfo;
      bool killsEverything;
      bool killsAddressStatics;
      bool killsIntStatics;
      bool killsNonIntPrimitiveStatics;
      bool killsAddressFields;
      bool killsIntFields;
      bool killsNonIntPrimitiveFields;
      bool killsAddressArrayShadows;
      bool killsIntArrayShadows;
      bool killsNonIntPrimitiveArrayShadows;
      };

   RefinedAliasWalkCollector _refinedAliasWalkCollector;


public:

   static bool wantToPatchClassPointer(TR::Compilation *comp,
                                       const TR_OpaqueClassBlock *allegedClassPointer,
                                       const uint8_t *inCodeAt);

   bool wantToPatchClassPointer(const TR_OpaqueClassBlock *allegedClassPointer, const uint8_t *inCodeAt);

   bool wantToPatchClassPointer(const TR_OpaqueClassBlock *allegedClassPointer, const TR::Node *forNode);

   // --------------------------------------------------------------------------
   // GPU
   //
   void generateGPU();

   void dumpInvariant(CS2::ArrayOf<gpuParameter, TR::Allocator>::Cursor pit, NVVMIRBuffer &ir, bool isbufferalign);

   GPUResult dumpNVVMIR(TR::TreeTop *firstTreeTop, TR::TreeTop *lastTreeTop, TR_RegionStructure *loop, SharedSparseBitVector *blocksinLoop, ListBase<TR::AutomaticSymbol> *autos, ListBase<TR::ParameterSymbol> *parms, bool staticMethod, char * &nvvmIR, TR::Node * &errorNode, int gpuPtxCount, bool* hasExceptionChecks);

   GPUResult printNVVMIR(NVVMIRBuffer &ir, TR::Node * node, TR_RegionStructure *loop, TR_BitVector *targetBlocks, vcount_t visitCount, TR_SharedMemoryAnnotations *sharedMemory, int32_t &nextParmNum, TR::Node * &errorNode);

   void findExtraParms(TR::Node *node, int32_t &numExtraParms, TR_SharedMemoryAnnotations *sharedMemory, vcount_t visitCount);

   bool handleRecognizedMethod(TR::Node *node, NVVMIRBuffer &ir, TR::Compilation *comp);

   bool handleRecognizedField(TR::Node *node, NVVMIRBuffer &ir);

   void printArrayCopyNVVMIR(TR::Node *node, NVVMIRBuffer &ir, TR::Compilation *comp);


   bool hasFixedFrameC_CallingConvention() {return _j9Flags.testAny(HasFixedFrameC_CallingConvention);}
   void setHasFixedFrameC_CallingConvention() {_j9Flags.set(HasFixedFrameC_CallingConvention);}

   bool supportsMethodEntryPadding();

   /** \brief
    *     Determines whether the code generator must generate the switch to interpreter snippet in the preprologue.
    */
   bool mustGenerateSwitchToInterpreterPrePrologue();

   bool buildInterpreterEntryPoint() { return true; }
   void generateCatchBlockBBStartPrologue(TR::Node *node, TR::Instruction *fenceInstruction);
   bool supportsUnneededLabelRemoval() { return false; }

   // Determines whether high-resolution timer can be used to implement java/lang/System.currentTimeMillis()
   bool getSupportsMaxPrecisionMilliTime() {return _j9Flags.testAny(SupportsMaxPrecisionMilliTime);}
   void setSupportsMaxPrecisionMilliTime() {_j9Flags.set(SupportsMaxPrecisionMilliTime);}

private:

   enum // Flags
      {
      HasFixedFrameC_CallingConvention = 0x00000001,
      SupportsMaxPrecisionMilliTime    = 0x00000002
      };

   flags32_t _j9Flags;
   };
}

#endif
