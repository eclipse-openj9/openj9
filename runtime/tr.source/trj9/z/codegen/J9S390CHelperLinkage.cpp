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

#include "trj9/z/codegen/J9S390PrivateLinkage.hpp"
#include "trj9/z/codegen/J9S390CHelperLinkage.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/Snippet.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/CHTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/J2IThunk.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "env/j9method.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/InterferenceGraph.hpp"
#include "z/codegen/OpMemToMem.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390HelperCallSnippet.hpp"
#include "z/codegen/S390J9CallSnippet.hpp"
#include "z/codegen/S390StackCheckFailureSnippet.hpp"
#include "z/codegen/TRSystemLinkage.hpp"
#include "runtime/J9Profiler.hpp"
#include "runtime/J9ValueProfiler.hpp"

////////////////////////////////////////////////////////////////////////////////
// TR::S390CHelperLinkage for J9
////////////////////////////////////////////////////////////////////////////////
TR::S390CHelperLinkage::S390CHelperLinkage(TR::CodeGenerator * codeGen,TR_S390LinkageConventions elc, TR_LinkageConventions lc)
   : TR::Linkage(codeGen,elc,lc)
   {
   //Preserved Registers

   static const bool enableVectorLinkage = codeGen->getSupportsVectorRegisters();
   setRegisterFlag(TR::RealRegister::GPR8, Preserved);
   setRegisterFlag(TR::RealRegister::GPR9, Preserved);
   setRegisterFlag(TR::RealRegister::GPR10, Preserved);
   setRegisterFlag(TR::RealRegister::GPR11, Preserved);
   setRegisterFlag(TR::RealRegister::GPR13, Preserved);
   setRegisterFlag(TR::RealRegister::GPR15, Preserved);

   if (TR::Compiler->target.is64Bit())
      {
      setRegisterFlag(TR::RealRegister::HPR6, Preserved);
      setRegisterFlag(TR::RealRegister::HPR7, Preserved);
      setRegisterFlag(TR::RealRegister::HPR8, Preserved);
      setRegisterFlag(TR::RealRegister::HPR9, Preserved);
      setRegisterFlag(TR::RealRegister::HPR10, Preserved);
      setRegisterFlag(TR::RealRegister::HPR11, Preserved);
      setRegisterFlag(TR::RealRegister::HPR12, Preserved);
      setRegisterFlag(TR::RealRegister::HPR15, Preserved);
      }


#if defined(ENABLE_PRESERVED_FPRS)
   // In case of 32 Bit Linux on Z, System Linkage only preserves FPR4 and FPR6, for all other target, FPR8-FPR15 is preserved. 
   if (TR::Compiler->target.isLinux() && TR::Compiler->target.is32Bit())
      {
      setRegisterFlag(TR::RealRegister::FPR4, Preserved);
      setRegisterFlag(TR::RealRegister::FPR6, Preserved);
      }
   else
      {
      setRegisterFlag(TR::RealRegister::FPR8, Preserved);
      setRegisterFlag(TR::RealRegister::FPR9, Preserved);
      setRegisterFlag(TR::RealRegister::FPR10, Preserved);
      setRegisterFlag(TR::RealRegister::FPR11, Preserved);
      setRegisterFlag(TR::RealRegister::FPR12, Preserved);
      setRegisterFlag(TR::RealRegister::FPR13, Preserved);
      setRegisterFlag(TR::RealRegister::FPR14, Preserved);
      setRegisterFlag(TR::RealRegister::FPR15, Preserved);
      }
#endif
   // Following Snippet for marking ARs preserved was taken from System Linkage.
   // Current linkage do not preserve the ARs
   if (TR::Compiler->target.isLinux())
      {
      
      // For Linux on Z, GPR6 and GPR7 is preserved by System Linkage but on zOS they are clobbered.
      setRegisterFlag(TR::RealRegister::GPR6, Preserved);
      setRegisterFlag(TR::RealRegister::GPR7, Preserved);
      setRegisterFlag(TR::RealRegister::GPR12, Preserved);

      setRegisterFlag(TR::RealRegister::AR8, Preserved);
      setRegisterFlag(TR::RealRegister::AR9, Preserved);
      setRegisterFlag(TR::RealRegister::AR10, Preserved);
      setRegisterFlag(TR::RealRegister::AR11, Preserved);
      setRegisterFlag(TR::RealRegister::AR12, Preserved);
      setRegisterFlag(TR::RealRegister::AR13, Preserved);
      setRegisterFlag(TR::RealRegister::AR14, Preserved);
      setRegisterFlag(TR::RealRegister::AR15, Preserved);

      setReturnAddressRegister (TR::RealRegister::GPR14 );
      setIntegerReturnRegister (TR::RealRegister::GPR2 );
      setLongReturnRegister    (TR::RealRegister::GPR2 );
      setIntegerArgumentRegister(0, TR::RealRegister::GPR2);
      setIntegerArgumentRegister(1, TR::RealRegister::GPR3);
      setIntegerArgumentRegister(2, TR::RealRegister::GPR4);
      setIntegerArgumentRegister(3, TR::RealRegister::GPR5);
      setIntegerArgumentRegister(4, TR::RealRegister::GPR6);
      // Hardcoding register map for GC can lead to subtle bugs if linkage is changed and we did not change register map for GC.
      // TODO We should write a function that goes through the list of preserved registers and prepare register map for GC.
      setPreservedRegisterMapForGC(0x0000BFC0);
      // GPR5 needs to be stored before using it as argument register.
      // Right now setting number of integer argument registers to 3.
      // TODO When we add support for more than 3 arguments, we should change this number to 5.
      setNumIntegerArgumentRegisters(3);
      }
   else 
      {
      // 31-Bit zOS will need GPR12 for CAA register so it won't be preserved. For all other variant it is preserved
      if (TR::Compiler->target.is64Bit())
         {
         setPreservedRegisterMapForGC(0x0000FF00); 
         setRegisterFlag(TR::RealRegister::GPR12, Preserved);
         }
      else
         {
         setPreservedRegisterMapForGC(0x0000EF00); 
         }
      if (enableVectorLinkage)
         {
         setRegisterFlag(TR::RealRegister::VRF16, Preserved);
         setRegisterFlag(TR::RealRegister::VRF17, Preserved);
         setRegisterFlag(TR::RealRegister::VRF18, Preserved);
         setRegisterFlag(TR::RealRegister::VRF19, Preserved);
         setRegisterFlag(TR::RealRegister::VRF20, Preserved);
         setRegisterFlag(TR::RealRegister::VRF21, Preserved);
         setRegisterFlag(TR::RealRegister::VRF22, Preserved);
         setRegisterFlag(TR::RealRegister::VRF23, Preserved);
         }

      setRegisterFlag(TR::RealRegister::GPR14, Preserved);

      setReturnAddressRegister (TR::RealRegister::GPR7 );
      setIntegerReturnRegister (TR::RealRegister::GPR3 );
      setLongReturnRegister    (TR::RealRegister::GPR3 );
      setIntegerArgumentRegister(0, TR::RealRegister::GPR1);
      setIntegerArgumentRegister(1, TR::RealRegister::GPR2);
      setIntegerArgumentRegister(2, TR::RealRegister::GPR3);
      setNumIntegerArgumentRegisters(3);
      }

   
   setLongLowReturnRegister (TR::RealRegister::GPR3 );
   setLongHighReturnRegister(TR::RealRegister::GPR2 );
   setFloatReturnRegister   (TR::RealRegister::FPR0 );
   setDoubleReturnRegister  (TR::RealRegister::FPR0 );
   setLongDoubleReturnRegister0  (TR::RealRegister::FPR0 );
   setLongDoubleReturnRegister2  (TR::RealRegister::FPR2 );
   setLongDoubleReturnRegister4  (TR::RealRegister::FPR4 );
   setLongDoubleReturnRegister6  (TR::RealRegister::FPR6 );
   setStackPointerRegister  (TR::RealRegister::GPR5 );
   setMethodMetaDataRegister(TR::RealRegister::GPR13 );
#if defined(J9ZOS390)
	setDSAPointerRegister (TR::RealRegister::GPR4 );
#if defined(TR_HOST_32BIT)
	setCAAPointerRegister (TR::RealRegister::GPR12 );
#endif
#endif
 
   }


// Utility to Manage RealRegisters for Helper Call Using C Linkage
/**   \brief Utility to Manage RealRegisters for Helper Call Using C Linkage.
 *    \details
 *    It is used to allocate a virtual register and assign them to the requested real register.
 */
class RealRegisterManager
   {
   public:
   RealRegisterManager(TR::CodeGenerator *cg) :
      _cg(cg),
      _numberOfRegistersInUse(0)
      {
      memset(_Registers, 0x0, sizeof(_Registers));
      }

   ~RealRegisterManager()
      {
      for(uint8_t i = (uint8_t)TR::RealRegister::NoReg; i < (uint8_t)TR::RealRegister::NumRegisters; i++)
         {
         if (_Registers[i] != NULL)
            {
            _cg->stopUsingRegister(_Registers[i]);
            }
         }
      }
   TR::Register* use(TR::RealRegister::RegNum RealRegister)
      {
      if (_Registers[RealRegister] == NULL)
         {
         if (RealRegister >= TR::RealRegister::FirstFPR && RealRegister <= TR::RealRegister::LastFPR)
            _Registers[RealRegister] = _cg->allocateRegister(TR_FPR);
         else if (RealRegister >= TR::RealRegister::FirstVRF && RealRegister <= TR::RealRegister::LastVRF)
            _Registers[RealRegister] = _cg->allocateRegister(TR_VRF);
         else if (!(RealRegister >= TR::RealRegister::FirstAR && RealRegister <= TR::RealRegister::LastAR))
            _Registers[RealRegister] = _cg->allocateRegister();
         else
            return NULL;
         _numberOfRegistersInUse++;
         }
      return _Registers[RealRegister];
      }
   // For the post dependency conditions we need return address register to be assigned to proper virtual register instead of the dummy virtual register
   TR::RegisterDependencyConditions* buildRegisterDependencyConditions(TR::RealRegister::RegNum returnAddressReg)
   	{
      // Helper function to generate RegisterDependencyCondition adds one more to post deps for return address.
      // Hence asking one less post deps so this won't create a problem when combined with other dependency condition for ICF.
   	TR::RegisterDependencyConditions* deps = generateRegisterDependencyConditions(0, numberOfRegistersInUse()-1, _cg);
   	for (uint8_t i = (uint8_t)TR::RealRegister::NoReg; i < (uint8_t)TR::RealRegister::NumRegisters; i++)
   		{
   		if (_Registers[i] != NULL)
   			{
   			if (i != returnAddressReg)
   				_Registers[i]->setPlaceholderReg();
   			deps->addPostCondition(_Registers[i], (TR::RealRegister::RegNum)i, DefinesDependentRegister);
   			}
   		}
   	return deps;
   	}
   
   inline uint8_t numberOfRegistersInUse() const
      {
      return _numberOfRegistersInUse;
      }
   private:
   uint8_t              _numberOfRegistersInUse;
   TR::Register*        _Registers[TR::RealRegister::NumRegisters];
   TR::CodeGenerator*   _cg;
   };

/**   \brief Build a JIT helper call.
 *    \details
 *    It generates sequence that prepares parameters for the JIT helper function and generate a helper call.
 *    \param callNode The node for which you are generating a heleper call
 *    \param deps The pre register dependency conditions that will be filled by this function to attach within ICF
 *    \param paramInRegisters Stack of registers that contains arguments to be passed to the helper function,
 *           if passed empty stack, this function will clobber evaluate children of node to prepare arguments
 *    \param returnReg TR::Register* allocated by consumer of this API to hold the result of the helper call,
 *           If passed, this function uses it to store return value from helper instead of allocating new register
 *    \return TR::Register *helperReturnResult, gets the return value of helper function and return to the evaluator. 
 */
TR::Register * TR::S390CHelperLinkage::buildDirectDispatch(TR::Node * callNode, TR::RegisterDependencyConditions **deps, TR_Stack<TR::Register*>& paramInRegisters, TR::Register *returnReg)
   {
   RealRegisterManager RealRegisters(cg());
   bool isHelperCallWithinICF = deps != NULL;
   // TODO: Currently only jitInstanceOf is fast path helper. Need to modify following condition if we add support for other fast path only helpers
   bool isFastPathOnly = callNode->getOpCodeValue() == TR::instanceof;
   traceMsg(comp(),"%s: Internal Control Flow in OOL : %s\n",callNode->getOpCode().getName(),isHelperCallWithinICF  ? "true" : "false" );
   for (int i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastHPR; i++)
      {
      if (!self()->getPreserved(REGNUM(i)) && cg()->machine()->getS390RealRegister(i)->getState() != TR::RealRegister::Locked)
         {
         RealRegisters.use((TR::RealRegister::RegNum)i);
         }
      }
   TR::RegisterDependencyConditions *childNodeRegDeps = NULL;
   // An empty list passed here means we need to evaluate child of the node to prepare argument lists.
   if (paramInRegisters.isEmpty())
      {
      childNodeRegDeps = generateRegisterDependencyConditions(0,callNode->getNumChildren(), cg());
      // TODO For Time Being, it is expected that param number won't increase beyond 3 need to fix this when support for stack is there
      for (int i=0; i< callNode->getNumChildren(); i++)
         {
         if (i < self()->getNumIntegerArgumentRegisters())
            paramInRegisters.push(cg()->gprClobberEvaluate(callNode->getChild(i)));
         else
            TR_ASSERT(false,"Parameters on Stack not supported yet");
         childNodeRegDeps->addPostConditionIfNotAlreadyInserted(callNode->getChild(i)->getRegister(), TR::RealRegister::AssignAny);
         }
      }
   
   TR::Register *vmThreadRegister = cg()->getMethodMetaDataRealRegister();
   TR::RegisterDependencyConditions* preDeps = generateRegisterDependencyConditions(paramInRegisters.size()+1, 0, cg());
   TR::Register *vmThreadArgReg = cg()->allocateRegister();
   
   // First argument to any helper function is vmThread
   generateRRInstruction(cg(), TR::InstOpCode::getLoadRegOpCode(), callNode, vmThreadArgReg, vmThreadRegister);
   preDeps->addPreCondition(vmThreadArgReg, getIntegerArgumentRegister(0));
   
   int iter=1;
   while (!paramInRegisters.isEmpty())
      preDeps->addPreCondition(paramInRegisters.pop(), getIntegerArgumentRegister(iter++));
   
   TR::Register *javaStackPointerRegister = NULL;
  /* On zOS, we need to use GPR7 as return address for fastPath helper calls
   * Following line will use the System linkage's return address register for fast path
   * And for regular dual mode helper, private linkage's return address register
   */ 
   TR::RealRegister::RegNum regRANum = isFastPathOnly ? self()->getReturnAddressRegister() : cg()->getReturnAddressRegister();
   TR::Register *regRA = RealRegisters.use(regRANum);
#if defined(J9ZOS390)
   TR::Register *DSAPointerReg = NULL;
   uint32_t offsetOfSSP = static_cast<uint32_t>(offsetof(J9VMThread, systemStackPointer));
   uint32_t pointerSize = TR::Compiler->target.is64Bit() ? 7 : 3;
#endif

   // Filling up the pre-dependencies for the helper call
   TR::RegisterDependencyConditions * postDeps = RealRegisters.buildRegisterDependencyConditions(regRANum);
   // If buildDirectDispatch is called within ICF we need to pass the dependencies which will be used there, else we need single dependencylist to be attached to the BRASL
   // We return postdependency conditions back to evaluator to merge with ICF condition and attach to merge label 
   if (isHelperCallWithinICF )
      *deps = childNodeRegDeps == NULL ? postDeps : new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(postDeps, childNodeRegDeps, cg());
   
   int padding = 0;
   uint32_t offsetJ9SP =  static_cast<uint32_t>(offsetof(J9VMThread, sp));
   TR::Instruction *cursor = NULL;

   /* Following routine will generate assembly routine as following
    * #if (fastPathHelper)
    *    STG R5, @(vmThread+offsetOf(J9SP))
    *       #define zOS
    *       LG DSA, @(vmThraed,offsetOfSSP)
    *       XC offsetOFSSP(vmThread, pointerSize), offsetOfSSP(vmThread)
    *          #define 32Bit
    *          LG CAA, @(DSA+CAAOffset)
    * #endif
    * BRASL R14,JIThelper
    * #if fastPathHelper
    *    #define zOS
    *       NOP // Padding for zOS return from helper
    *       STG DSA, @(vmThread, offsetOfSSP)
    *    LG R5, @(vmThread, offsetOfJ9SP)
    * #endif      
    */

   if (isFastPathOnly)
      {
      // Storing Java Stack Pointer
      javaStackPointerRegister = cg()->getStackPointerRealRegister();
      cursor = generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), callNode, javaStackPointerRegister, generateS390MemoryReference(vmThreadRegister, offsetJ9SP, cg()));
#if defined(J9ZOS390)
      padding += 2;
      // Loading DSAPointer Register
      DSAPointerReg = RealRegisters.use(getDSAPointerRegister());
      generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), callNode, DSAPointerReg, generateS390MemoryReference(vmThreadRegister, offsetOfSSP, cg()));
      generateSS1Instruction(cg(), TR::InstOpCode::XC, callNode, pointerSize,
         generateS390MemoryReference(vmThreadRegister, offsetOfSSP, cg()),
	 generateS390MemoryReference(vmThreadRegister, offsetOfSSP, cg()));
#if defined(TR_HOST_32BIT)
      padding += 2;
      // Loading CAA Pointer Register
      TR::Register *CAAPointerReg = RealRegisters.use(getCAAPointerRegister());
      TR_J9VMBase *fej9 = (TR_J9VMBase *) cg()->comp()->fe();
      int32_t J9TR_CAA_save_offset = fej9->getCAASaveOffset();
      generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), callNode, CAAPointerReg, generateS390MemoryReference(DSAPointerReg, J9TR_CAA_save_offset, cg()));
#endif
#endif
      }
   TR::SymbolReference * callSymRef = callNode->getSymbolReference();
   intptrj_t destAddr = (intptrj_t) callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->getMethodAddress();
   cursor = new (cg()->trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::BRASL, callNode, regRA, destAddr, callSymRef, cg());
   cursor->setDependencyConditions(preDeps);
   if (isFastPathOnly)
      {
#if defined(J9ZOS390)
      // We need following padding as return from C function will skip the XPLink eyecatcher, padding ensures entry to valid instruction 
      cursor = new (cg()->trHeapMemory()) TR::S390NOPInstruction(TR::InstOpCode::NOP, padding, callNode, cursor, cg());
      // Storing DSA Register back
      cursor = generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), callNode, DSAPointerReg, generateS390MemoryReference(vmThreadRegister, offsetOfSSP, cg()), cursor);
#endif
      // Reloading Java Stack pointer
      cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), callNode, javaStackPointerRegister, generateS390MemoryReference(vmThreadRegister, offsetJ9SP, cg()), cursor);
      }
   else
      {
      // Fastpath helper do not expects GC call in-between so only attaching them for normal dual mode helpers
      // As GC map is attached to instruction after RA is done, it is guaranteed that all the non-preserved register by system linkage are either stored in preserved register
      // Or spilled to stack. We only need to mark preserved register in GC map. Only possiblity of non-preserved register containing a live object is in argument to helper which should be a clobberable copy of actual object. 
      cursor->setNeedsGCMap(getPreservedRegisterMapForGC());
      }

   // If helper call is fast path helper call and is not within ICF,
   // We need to attach post dependency to the restoring of java stack pointer.
   // This will assure that reverse spilling occurs after restoring of java stack pointer
   if (!isHelperCallWithinICF)
      cursor->setDependencyConditions(postDeps);
   preDeps->stopUsingDepRegs(cg());
   // Logic To Handle Return Register
   TR::DataType returnType = callNode->getDataType();
   if (returnReg == NULL)
      {
      switch(returnType)
         {
         case TR::NoType:
            traceMsg(comp(), "ReturnType = %s\n",returnType.toString());
            break;
         case TR::Address:
            traceMsg(comp(), "ReturnType = %s\n",returnType.toString());
            returnReg = cg()->allocateCollectedReferenceRegister();
            break;
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
#ifdef TR_TARGET_64BIT
         case TR::Int64:
#endif
            traceMsg(comp(), "ReturnType = %s\n",returnType.toString());
            returnReg = cg()->allocateRegister();
            break;
         default:
            TR_ASSERT(false, "Unsupported Call return data type: %s\n", returnType.toString());
            break;
         }
      }
   // We need to fill returnReg only if it requested by evaluator or node returns value or address
   if (returnReg != NULL)
      generateRRInstruction(cg(), TR::InstOpCode::getLoadRegOpCode(), callNode, returnReg,  RealRegisters.use(isFastPathOnly ? getIntegerReturnRegister():getLongHighReturnRegister()), cursor);
   
   return returnReg;
   }
