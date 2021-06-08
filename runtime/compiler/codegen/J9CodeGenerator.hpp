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

#ifndef J9_CODEGENERATOR_INCL
#define J9_CODEGENERATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_CODEGENERATOR_CONNECTOR
#define J9_CODEGENERATOR_CONNECTOR

namespace J9 { class CodeGenerator; }
namespace J9 { typedef J9::CodeGenerator CodeGeneratorConnector; }
#endif

#include "codegen/OMRCodeGenerator.hpp"

#include <stdint.h>
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "infra/List.hpp"
#include "infra/HashTab.hpp"
#include "codegen/RecognizedMethods.hpp"
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationRuntime.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "optimizer/Dominators.hpp"
#include "cs2/arrayof.h"

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

protected:

   CodeGenerator(TR::Compilation *comp);

public:

   void initialize();

   TR_J9VMBase *fej9();

   TR::TreeTop *lowerTree(TR::Node *root, TR::TreeTop *treeTop);

   void preLowerTrees();

   void lowerTreesPreTreeTopVisit(TR::TreeTop *tt, vcount_t visitCount);

   void lowerTreesPreChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount);

   void lowerTreeIfNeeded(TR::Node *node, int32_t childNumber, TR::Node *parent, TR::TreeTop *tt);

   void lowerDualOperator(TR::Node *parent, int32_t childNumber, TR::TreeTop *treeTop);

public:

   bool collectSymRefs(TR::Node *node, TR_BitVector *symRefs, vcount_t secondVisitCount);

   void moveUpArrayLengthStores(TR::TreeTop *insertionPoint);

   void doInstructionSelection();

   void createReferenceReadBarrier(TR::TreeTop* treeTop, TR::Node* parent);

   TR::list<TR_Pair<TR_ResolvedMethod,TR::Instruction> *> &getJNICallSites() { return _jniCallSites; }  // registerAssumptions()

   // OSR, not code generator
   void populateOSRBuffer();

   void lowerCompressedRefs(TR::TreeTop *, TR::Node*, vcount_t, TR_BitVector *);
   void compressedReferenceRematerialization(); // J9
   void rematerializeCompressedRefs(TR::SymbolReference * &, TR::TreeTop *, TR::Node*, int32_t, TR::Node*, vcount_t, List<TR::Node> *);
   void anchorRematNodesIfNeeded(TR::Node*, TR::TreeTop *, List<TR::Node> *);
   void yankCompressedRefs(TR::TreeTop *, TR::Node *, int32_t, TR::Node *, vcount_t, vcount_t);

   void setUpForInstructionSelection();

   void insertEpilogueYieldPoints();

   void allocateLinkageRegisters();

   void fixUpProfiledInterfaceGuardTest();

   /**
    * \brief
    *      This query is used by both fixUpProfiledInterfaceGuardTest (a codegen level optimization) and virtual guard evaluators
    *      to decide whether a NOP guard should be generated. It's used on all platforms and compilation phases so that the decision
    *      of generating VG NOPs is made in a consistent way.
    *
    * \param node
    *      the virtual guard node
    *
    * \return
    *      true if a NOP virtual guard should be generated. Otherwise, false.
   */
   bool willGenerateNOPForVirtualGuard(TR::Node* node);

   void zeroOutAutoOnEdge(TR::SymbolReference * liveAutoSym, TR::Block *block, TR::Block *succBlock, TR::list<TR::Block*> *newBlocks, TR_ScratchList<TR::Node> *fsdStores);

   TR::Linkage *createLinkageForCompilation();

   bool enableAESInHardwareTransformations() {return false;}

   bool isMethodInAtomicLongGroup(TR::RecognizedMethod rm);
   bool arithmeticNeedsLiteralFromPool(TR::Node *node) { return false; }

   // OSR
   //
   TR::TreeTop* genSymRefStoreToArray(TR::Node* refNode, TR::Node* arrayAddressNode, TR::Node* firstOffset,
                                      TR::Node* symRefLoad, int32_t secondOffset,  TR::TreeTop* insertionPoint);

   // --------------------------------------
   // AOT Relocations
   //
#if defined(J9VM_OPT_JITSERVER)
   void addExternalRelocation(TR::Relocation *r, const char *generatingFileName, uintptr_t generatingLineNumber, TR::Node *node, TR::ExternalRelocationPositionRequest where = TR::ExternalRelocationAtBack);
   void addExternalRelocation(TR::Relocation *r, TR::RelocationDebugInfo *info, TR::ExternalRelocationPositionRequest where = TR::ExternalRelocationAtBack);
#endif /* defined(J9VM_OPT_JITSERVER) */

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
   bool needRelocationsForLookupEvaluationData();
   bool needRelocationsForStatics();
   bool needRelocationsForHelpers();
   bool needRelocationsForCurrentMethodPC();
#if defined(J9VM_OPT_JITSERVER)
   bool needRelocationsForBodyInfoData();
   bool needRelocationsForPersistentInfoData();
#endif /* defined(J9VM_OPT_JITSERVER) */

   // ----------------------------------------
   TR::Node *createOrFindClonedNode(TR::Node *node, int32_t numChildren);

   void jitAddUnresolvedAddressMaterializationToPatchOnClassRedefinition(void *firstInstruction);

   uint32_t getStackLimitOffset() {return _stackLimitOffsetInMetaData;}
   uint32_t setStackLimitOffset(uint32_t o) {return (_stackLimitOffsetInMetaData = o);}

   bool alwaysGeneratesAKnownCleanSign(TR::Node *node) { return false; } // no virt
   bool alwaysGeneratesAKnownPositiveCleanSign(TR::Node *node) { return false; } // no virt
   TR_RawBCDSignCode alwaysGeneratedSign(TR::Node *node) { return raw_bcd_sign_unknown; } // no virt

   void swapChildrenIfNeeded(TR::Node *store, char *optDetails);

   TR::AutomaticSymbol *allocateVariableSizeSymbol(int32_t size);
   TR::SymbolReference *allocateVariableSizeSymRef(int32_t byteLength);
   void pendingFreeVariableSizeSymRef(TR::SymbolReference *sym);
   void freeVariableSizeSymRef(TR::SymbolReference *sym, bool freeAddressTakenSymbol=false);
   void freeAllVariableSizeSymRefs();

   TR::SymbolReference *getFreeVariableSizeSymRef(int byteLength);
   void checkForUnfreedVariableSizeSymRefs();

   bool allowGuardMerging();
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
   uintptr_t objectLengthOffset();
   uintptr_t objectHeaderInvariant();

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

   TR_BitVector *getLiveMonitors() {return _liveMonitors;}
   TR_BitVector *setLiveMonitors(TR_BitVector *v) {return (_liveMonitors = v);}

public:
   /*
    * \brief
    *    Get the most abstract type the monitor may be operating on.
    *
    * \note
    *    java.lang.Object is only returned when the monitor object is of type java.lang.Object but not any subclasses
    */
   TR_OpaqueClassBlock* getMonClass(TR::Node* monNode);

   /*
    * \brief
    *    Whether a monitor object is of value type
    *
    * \return
    *    TR_yes The monitor object is definitely value type
    *    TR_no The monitor object is definitely identity type
    *    TR_maybe It is unknown whether the monitor object is identity type or value type
    */
   TR_YesNoMaybe isMonitorValueType(TR::Node* monNode);
   /*
    * \brief
    *    Whether a monitor object is of value based class type or value type.
    *    This API checks if value based or value type is enabled first.
    *
    * \return
    *    TR_yes The monitor object is definitely value based class type or value type
    *    TR_no The monitor object is definitely not value based class type or value type
    *    TR_maybe It is unknown whether the monitor object is value based class type or value type
    */
   TR_YesNoMaybe isMonitorValueBasedOrValueType(TR::Node* monNode);

protected:

typedef TR::typed_allocator<std::pair<const ncount_t , TR_OpaqueClassBlock*>, TR::Region &> MonitorMapAllocator;
typedef std::map<ncount_t , TR_OpaqueClassBlock *, std::less<ncount_t>, MonitorMapAllocator> MonitorTypeMap;

MonitorTypeMap _monitorMapping; // map global node index to monitor object class

void addMonClass(TR::Node* monNode, TR_OpaqueClassBlock* clazz);

private:

   TR_HashTabInt _uncommonedNodes;               // uncommoned nodes keyed by the original nodes

   TR::list<TR::Node*> _nodesSpineCheckedList;

   TR::list<TR_Pair<TR_ResolvedMethod, TR::Instruction> *> _jniCallSites; // list of instrutions representing direct jni call sites

   uint16_t changeParmLoadsToRegLoads(TR::Node*node, TR::Node **regLoads, TR_BitVector *globalRegsWithRegLoad, TR_BitVector &killedParms, vcount_t visitCount); // returns number of RegLoad nodes created

   static bool wantToPatchClassPointer(TR::Compilation *comp,
                                       const TR_OpaqueClassBlock *allegedClassPointer,
                                       const char *locationDescription,
                                       const void *location)
      {
      // If we have a class pointer to consider, it should look like one.
      const uintptr_t j9classEyecatcher = 0x99669966;
#if defined(J9VM_OPT_JITSERVER)
      if (allegedClassPointer != NULL && !comp->isOutOfProcessCompilation())
#else
      if (allegedClassPointer != NULL)
#endif /* defined(J9VM_OPT_JITSERVER) */
         {
         TR_ASSERT(*(const uintptr_t*)allegedClassPointer == j9classEyecatcher,
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

   TR_BitVector *_liveMonitors;

protected:

   // isTemporaryBased storageReferences just have a symRef but some other routines expect a node so use the below to fill in this symRef on this node
   TR::Node *_dummyTempStorageRefNode;

public:

   static bool wantToPatchClassPointer(TR::Compilation *comp,
                                       const TR_OpaqueClassBlock *allegedClassPointer,
                                       const uint8_t *inCodeAt);

   bool wantToPatchClassPointer(const TR_OpaqueClassBlock *allegedClassPointer, const uint8_t *inCodeAt);

   bool wantToPatchClassPointer(const TR_OpaqueClassBlock *allegedClassPointer, const TR::Node *forNode);

   bool getSupportsBigDecimalLongLookasideVersioning() { return _flags3.testAny(SupportsBigDecimalLongLookasideVersioning);}
   void setSupportsBigDecimalLongLookasideVersioning() { _flags3.set(SupportsBigDecimalLongLookasideVersioning);}

   bool constLoadNeedsLiteralFromPool(TR::Node *node) { return false; }

   // Java, likely Z
   bool supportsTrapsInTMRegion() { return true; }

   // J9
   int32_t getInternalPtrMapBit() { return 31;}

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

   bool supportsJitMethodEntryAlignment();

   /** \brief
    *     Determines whether the code generator supports inlining of java/lang/Class.isAssignableFrom
    */
   bool supportsInliningOfIsAssignableFrom() { return false; } // no virt, default

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

   /** \brief
    *    Determines whether the code generator supports inlining of java/lang/String.toUpperCase() and toLowerCase()
    */
   bool getSupportsInlineStringCaseConversion() { return _j9Flags.testAny(SupportsInlineStringCaseConversion);}
   /** \brief
    *    The code generator supports inlining of java/lang/String.toUpperCase() and toLowerCase()
    */
   void setSupportsInlineStringCaseConversion() { _j9Flags.set(SupportsInlineStringCaseConversion);}

   /** \brief
    *    Determines whether the code generator supports inlining of java/lang/String.indexOf()
    */
   bool getSupportsInlineStringIndexOf() { return _j9Flags.testAny(SupportsInlineStringIndexOf);}

   /** \brief
    *    The code generator supports inlining of java/lang/String.indexOf()
    */
   void setSupportsInlineStringIndexOf() { _j9Flags.set(SupportsInlineStringIndexOf);}

   /** \brief
   *    Determines whether the code generator supports inlining of java/lang/String.hashCode()
   */
   bool getSupportsInlineStringHashCode() { return _j9Flags.testAny(SupportsInlineStringHashCode); }

   /** \brief
   *    The code generator supports inlining of java/lang/String.hashCode()
   */
   void setSupportsInlineStringHashCode() { _j9Flags.set(SupportsInlineStringHashCode); }

   /** \brief
   *    Determines whether the code generator supports inlining of java/lang/StringLatin1.inflate
   */
   bool getSupportsInlineStringLatin1Inflate() { return _j9Flags.testAny(SupportsInlineStringLatin1Inflate); }

   /** \brief
   *    The code generator supports inlining of java/lang/StringLatin1.inflate
   */
   void setSupportsInlineStringLatin1Inflate() { _j9Flags.set(SupportsInlineStringLatin1Inflate); }

   /** \brief
   *    Determines whether the code generator supports inlining of java_util_concurrent_ConcurrentLinkedQueue_tm*
   *    methods
   */
   bool getSupportsInlineConcurrentLinkedQueue() { return _j9Flags.testAny(SupportsInlineConcurrentLinkedQueue); }

   /** \brief
   *    The code generator supports inlining of java_util_concurrent_ConcurrentLinkedQueue_tm* methods
   */
   void setSupportsInlineConcurrentLinkedQueue() { _j9Flags.set(SupportsInlineConcurrentLinkedQueue); }

   /**
    * \brief
    *    The number of nodes between a monext and the next monent before
    *    transforming a monitored region with transactional lock elision.
    */
   int32_t getMinimumNumberOfNodesBetweenMonitorsForTLE() { return 15; }

   /**
    * \brief Trim the size of code memory required by this method to match the
    *        actual code length required, allowing the reclaimed memory to be
    *        reused.  This is needed when the conservative length estimate
    *        exceeds the actual memory requirement.
    */
   void trimCodeMemoryToActualSize();


   /**
    * \brief Request and reserve a CodeCache for use by this compilation.  Fail
    *        the compilation appropriately if a CodeCache cannot be allocated.
    */
   void reserveCodeCache();


   /**
    * \brief Allocates code memory of the specified size in the specified area of
    *        the code cache.  Fail the compilation on failure.
    *
    * \param[in]  warmCodeSizeInBytes : the number of bytes to allocate in the warm area
    * \param[in]  coldCodeSizeInBytes : the number of bytes to allocate in the cold area
    * \param[out] coldCode : address of the cold code (if allocated)
    * \param[in]  isMethodHeaderNeeded : boolean indicating whether space for a
    *                method header must be allocated
    *
    * \return address of the allocated warm code (if allocated)
    */
   uint8_t *allocateCodeMemoryInner(
      uint32_t warmCodeSizeInBytes,
      uint32_t coldCodeSizeInBytes,
      uint8_t **coldCode,
      bool isMethodHeaderNeeded);


   /**
    * \brief Store a poison value in an auto slot that should have gone dead.  Used for debugging.
    *
    * \param[in] currentBlock : block in which the auto slot appears
    * \param[in] liveAutoSymRef : SymbolReference of auto slot to poison
    *
    * \return poisoned store node
    */
   TR::Node *generatePoisonNode(
      TR::Block *currentBlock,
      TR::SymbolReference *liveAutoSymRef);


   /**
    * \brief Determines whether VM Internal Natives is supported or not
    */
   bool supportVMInternalNatives();


   /**
    * \brief Determines whether the code generator supports stack allocations
    */
   bool supportsStackAllocations() { return false; }

   /**
    * \brief Initializes the Linkage Info word found before the interpreter entry point.
    *
    * \param[in] linkageInfo : pointer to the linkage info word
    *
    * \return Linkage Info word
    */
   uint32_t initializeLinkageInfo(void *linkageInfoPtr);

private:

   enum // Flags
      {
      HasFixedFrameC_CallingConvention                    = 0x00000001,
      SupportsMaxPrecisionMilliTime                       = 0x00000002,
      SupportsInlineStringCaseConversion                  = 0x00000004, /*! codegen inlining of Java string case conversion */
      SupportsInlineStringIndexOf                         = 0x00000008, /*! codegen inlining of Java string index of */
      SupportsInlineStringHashCode                        = 0x00000010, /*! codegen inlining of Java string hash code */
      SupportsInlineConcurrentLinkedQueue                 = 0x00000020,
      SupportsBigDecimalLongLookasideVersioning           = 0x00000040,
      SupportsInlineStringLatin1Inflate                   = 0x00000080, /*! codegen inlining of Java StringLatin1.inflate */
      };

   flags32_t _j9Flags;
   };
}

#endif
