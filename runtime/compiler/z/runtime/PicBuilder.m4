ifdef(`J9ZOS390',`dnl
define(`ZZ',`**')
',`dnl
define(`ZZ',`##')
')dnl

ZZ Copyright (c) 2000, 2019 IBM Corp. and others
ZZ
ZZ This program and the accompanying materials are made 
ZZ available under the terms of the Eclipse Public License 2.0 
ZZ which accompanies this distribution and is available at 
ZZ https://www.eclipse.org/legal/epl-2.0/ or the Apache License, 
ZZ Version 2.0 which accompanies this distribution and is available 
ZZ at https://www.apache.org/licenses/LICENSE-2.0.
ZZ
ZZ This Source Code may also be made available under the following
ZZ Secondary Licenses when the conditions for such availability set
ZZ forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
ZZ General Public License, version 2 with the GNU Classpath 
ZZ Exception [1] and GNU General Public License, version 2 with the
ZZ OpenJDK Assembly Exception [2].
ZZ
ZZ [1] https://www.gnu.org/software/classpath/license.html
ZZ [2] http://openjdk.java.net/legal/assembly-exception.html
ZZ
ZZ SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR
ZZ GPL-2.0 WITH Classpath-exception-2.0 OR
ZZ LicenseRef-GPL-2.0 WITH Assembly-exception

ZZ ===================================================================
ZZ The macro at the top replaces ZZ with appropriate line comment
ZZ For zLinux GAS assembler and zOS HLASM assembler
ZZ ===================================================================

ZZ ===================================================================
ZZ changequote is used to replace ` with [ and ' with ]
ZZ For better readability
ZZ ===================================================================
changequote([,])dnl

ZZ ===================================================================
ZZ For zOS, We declare just one csect with multiple entry points.
ZZ Each PICBuilder function will be declared as an entrypoint in this
ZZ csect.
ZZ ===================================================================

ifdef([J9ZOS390],[dnl
        TITLE 'PicBuilder.s'
PICBUILDER#START      CSECT
PICBUILDER#START      RMODE ANY
ifdef([TR_HOST_64BIT],[dnl
PICBUILDER#START      AMODE 64
],[dnl
PICBUILDER#START      AMODE 31
])dnl
])dnl

ZZ This file contains glue routines which are used to call out
ZZ to helper and interpreter.  Register usage is important in
ZZ the glue code and the following are the restriction.
ZZ
ZZ Stack walker care about the stack shape at every helper call.
ZZ
ZZ 1. For virtual and interface resolves,the stack walker requires
ZZ    that the argument registers be pushed (and nothing else),and
ZZ    that all preserved registers are live when the helper call is
ZZ    made.
ZZ
ZZ 2. For static and special resolves,the walker requires that
ZZ    all arguments have been correctly backstored into stack memory
ZZ    (done in the snippet),nothing extra is pushed on the stack,
ZZ    and that all preserved registers are live when the helper call
ZZ    is made.
ZZ
ZZ 3. For data resolves,the walker requires that all register mapped
ZZ    registers (all registers in the 390 case) be pushed (and nothing
ZZ    else).  The values in the registers when the helper call is made
ZZ    are irrelevant.
ZZ
ZZ 4. The static glue code (and any other code which does not call a
ZZ    resolve helper) can do whatever it likes,since no stack walk
ZZ    can occur while that code is running.
ZZ
ZZ As a rule of thumb,if the helper we're calling begins with "jit"
ZZ then stack walker concerned about the stack shape
ZZ ===================================================================

ZZ ===================================================================
ZZ codert/jilconsts.inc is a j9 assembler include that defines
ZZ offsets of various fields in structs used by jit
ZZ ===================================================================

include([jilconsts.inc])dnl

ZZ ===================================================================
ZZ codert.dev/s390_asdef.inc is a JIT assembler include that defines
ZZ macros for instructions for 64 bit and 31 bit mode. Appropriate set
ZZ of instruction macros are selected for each mode
ZZ ===================================================================

include([z/runtime/s390_asdef.inc])dnl


ZZ ===================================================================
ZZ codert.dev/s390_macros.inc is a JIT assembler include that defines
ZZ various m4 macros used in this file. It includes zOS_macros.inc on
ZZ zOS and zLinux_macros.inc on zLinux
ZZ ===================================================================

include([z/runtime/s390_macros.inc])dnl

ZZ ===================================================================
ZZ codert.dev/Helper.inc is a JIT assembler include that defines
ZZ index of various runtime helper functions addressable from
ZZ codertTOC.
ZZ ===================================================================

include([runtime/Helpers.inc])dnl

ZZ ===================================================================
ZZ Definitions for various offsets used by PicBuilder code
ZZ ===================================================================
ZZ Code base register for PIC method
SETVAL(breg,r10)

ZZ ===================================================================
ZZ Used for linkage from C calls
ZZ ===================================================================
ifdef([TR_HOST_64BIT],[dnl

ZZ ===================================================================
ZZ Offsets of items in various code snippets (64-bit mode)
ZZ ===================================================================

ZZ Unresolved Call Snippet Layout (64-bit mode)
SETVAL(eq_methodaddr_inSnippet,0)
SETVAL(eq_codeRA_inSnippet,8)
SETVAL(eq_methodptr_inSnippet,16)
SETVAL(eq_cp_inSnippet,24)
SETVAL(eq_cpindex_inSnippet,32)

ZZ Virtual Unresolved Call Snippet Layout (64-bit mode)
SETVAL(eq_methodaddr_inVUCallSnippet,0)
SETVAL(eq_codeRA_inVUCallSnippet,8)
SETVAL(eq_cp_inVUCallSnippet,16)
SETVAL(eq_cpindex_inVUCallSnippet,24)
SETVAL(eq_patchVftInstr_inVUCallSnippet,32)
SETVAL(eq_privMethod_inVUCallSnippet,40)
SETVAL(eq_j2i_thunk_inVUCallSnippet,48)
SETVAL(eq_privateRA_inVUCallSnippet,56)

ZZ These two should really be in codert/jilconsts.inc
SETVAL(J9TR_MethodConstantPool,8)
SETVAL(eq_ramclassFromConstantPool,0)

ZZ Unresolved Data Snippet Layout (64-bit mode)
SETVAL(eq_methodaddr_inDataSnippet,0)
SETVAL(eq_codeRA_inDataSnippet,8)
SETVAL(eq_cpindex_inDataSnippet,16)
SETVAL(eq_cp_inDataSnippet,20)
SETVAL(eq_codeRef_inDataSnippet,28)
SETVAL(eq_literalPoolAddr_inDataSnippet,36)
SETVAL(eq_offsetSlot_inDataSnippet,44)
SETVAL(eq_patchOffset_inDataSnippet,52)
SETVAL(eq_outOfLineStart_inDataSnippet,56)

ZZ  MethodFlags is a 32bit integer,but occupies 8 byte slot on 64bit
SETVAL(eq_methodFlagsOffset,(J9TR_MethodFlagsOffset+4))

ZZ The compiled flag is hidden in the LSB of the J9Method.extra field
SETVAL(eq_methodCompiledFlagOffset,(J9TR_MethodPCStartOffset+7))
],[dnl

ZZ ===================================================================
ZZ Offsets of items in various code snippets (31 bit mode)
ZZ ===================================================================

ZZ Unresolved Call Snippet Layout (31 bit mode)
SETVAL(eq_methodaddr_inSnippet,0)
SETVAL(eq_codeRA_inSnippet,4)
SETVAL(eq_methodptr_inSnippet,8)
SETVAL(eq_cp_inSnippet,12)
SETVAL(eq_cpindex_inSnippet,16)

ZZ Virtual Unresolved Call Snippet Layout (31 bit mode)
SETVAL(eq_methodaddr_inVUCallSnippet,0)
SETVAL(eq_codeRA_inVUCallSnippet,4)
SETVAL(eq_cp_inVUCallSnippet,8)
SETVAL(eq_cpindex_inVUCallSnippet,12)
SETVAL(eq_patchVftInstr_inVUCallSnippet,16)
SETVAL(eq_privMethod_inVUCallSnippet,20)
SETVAL(eq_j2i_thunk_inVUCallSnippet,24)
SETVAL(eq_privateRA_inVUCallSnippet,28)

ZZ Unresolved Data Snippet Layout (31 bit mode)
SETVAL(eq_methodaddr_inDataSnippet,0)
SETVAL(eq_codeRA_inDataSnippet,4)
SETVAL(eq_cpindex_inDataSnippet,8)
SETVAL(eq_cp_inDataSnippet,12)
SETVAL(eq_codeRef_inDataSnippet,16)
SETVAL(eq_literalPoolAddr_inDataSnippet,20)
SETVAL(eq_offsetSlot_inDataSnippet,24)
SETVAL(eq_patchOffset_inDataSnippet,28)
SETVAL(eq_outOfLineStart_inDataSnippet,32)

SETVAL(eq_methodFlagsOffset,J9TR_MethodFlagsOffset)

ZZ The compiled flag is hidden in the LSB of the J9Method.extra field
SETVAL(eq_methodCompiledFlagOffset,(J9TR_MethodPCStartOffset+3))
])dnl

SETPPA2()

ZZ ===================================================================
ZZ _interpreterUnresolved{Special,Static,VirtualDirect}Glue
ZZ
ZZ This glue function is called by an unresolved call snippet.  It in
ZZ turn calls a VM routine (i.e. jitResolve{Special,Static,Virtual-
ZZ Direct}Glue to resolve an unresolved method, patches the
ZZ invoking snippet with the resolved address, and returns to the
ZZ point where the call was originally made in the code cache.
ZZ
ZZ The various versions of the glue function share the same snippet
ZZ patching code, starting at label LUnresolved_common_code.
ZZ
ZZ Arguments to the VM Resolution (jitResolve*Method) are stored
ZZ in the snippet code.  The VM resolution routines have three
ZZ parameters:
ZZ                 r1:   codeCache RA
ZZ                 r2:   constant pool address
ZZ                 r3:   constant pool index
ZZ
ZZ For clinit's, we do not update the method address until it has
ZZ be resolved (i.e. LSB is not 1).  This forces all passes of
ZZ of the Snippet to go through the longer resolve path, instead
ZZ of patching.  The patched code is done through a call to a
ZZ static glue.
ZZ
ZZ An unresolved call snippet before resolution will look like:
ZZ             BASR rEP, 0          // Get current PC.
ZZ             LG   r14, 8(,rEP)    // Load glue method addr.
ZZ             BASR r14, r14        // Save Snippet Base and branch
ZZ                                  // to glue.
ZZ             DC <GlueMethodAddr>  // Glue Method Addr
ZZ             DC <CodeCache RA>    // Call Point Return Address
ZZ             DC <Method Ptr>
ZZ             DC <Const Pool Addr> // Constant Pool Addr
ZZ             DC <Const Pool Indx> // Constant Pool Index
ZZ
ZZ An unresolved call snippet after resolution will look like:
ZZ             BASR rEP, 0          // Get current PC.
ZZ             LG   r14, 8(,rEP)    // Load Resolved method addr.
ZZ             BASR r14, r14        // Save Snippet Base and branch
ZZ                                  // to method.
ZZ             DC <ResMethodAddr>   //*<---- Resolved Method Addr
ZZ             DC <CodeCache RA>    // Call Point Return Address
ZZ             DC <Method Ptr>
ZZ             DC <Const Pool Addr> // Constant Pool Addr
ZZ             DC <Const Pool Indx> // Constant Pool Index
ZZ ===================================================================
ZZ
ZZ ===================================================================
ZZ PICBuilder routine _interpreterUnresolvedSpecialGlue
ZZ
ZZ Exports:
ZZ   _interpreterUnresolvedSpecialGlue
ZZ
ZZ Description(s):
ZZ
ZZ   int _interpreterUnresolvedSpecialGlue
ZZ   =============================
ZZ   Invoke jitResolveSpecialMethod to resolve a method and
ZZ   update the snippet code
ZZ
ZZ   Preconditions:
ZZ     It expects the parameters to jitResolveSpecialMethod
ZZ     in the snippet code.
ZZ
ZZ   External References:
ZZ
ZZ   Register Usage:
ZZ     r6
ZZ     r7
ZZ
ZZ   Calls:
ZZ ===================================================================
START_FUNC(_interpreterUnresolvedSpecialGlue,intpUnrSpG)

LABEL(_interpreterUnresolvedSpecialGlue_BODY)

ZZ# set up snippet base register
    AlignSB(_intpUnrSpG_B)

ZZ# copy snippet base register
    LR_GPR  rSB,r14

ZZ# p1) Load code cache RA from the snippet
    L_GPR   r1,eq_codeRA_inSnippet(,rSB)

ZZ# p2) Load constant pool literal
    L_GPR   r2,eq_cp_inSnippet(,rSB)

ZZ# p3) Load cp index
    XR_GPR  r3,r3
    ICM     r3,7,eq_cpindex_inSnippet+1(rSB) # Load 3-byte cp index

LOAD_ADDR_FROM_TOC(r14,TR_S390jitResolveSpecialMethod)
    BASR    r14,r14       # Call jitResolveSpecialMethod

    J       LUnresolved_common_code

    END_FUNC(_interpreterUnresolvedSpecialGlue,intpUnrSpG,12)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterUnresolvedStaticGlue
ZZ
ZZ ===================================================================

    START_FUNC(_interpreterUnresolvedStaticGlue,intpUnrStG)

    AlignSB(_intpUnrStG_B)
    LR_GPR  rSB,r14                     # set up snippet base register

LABEL(_interpreterUnresolvedStaticGlue_BODY)

    L_GPR   r1,eq_codeRA_inSnippet(rSB) # p1) Load code cache RA
    L_GPR   r2,eq_cp_inSnippet(rSB)     # p2) Load constant pool lit.
    XR_GPR  r3,r3
    ICM     r3,7,eq_cpindex_inSnippet+1(rSB) # P3) Load 3-byte cp index

LOAD_ADDR_FROM_TOC(r14,TR_S390jitResolveStaticMethod)

    BASR    r14,r14                     # Call jitResolveStaticMethod

    J       LUnresolved_common_code

    END_FUNC(_interpreterUnresolvedStaticGlue,intpUnrStG,12)

ZZ ===================================================================
ZZ PICBuilder routine _interpreterUnresolvedDirectVirtualGlue
ZZ
ZZ ===================================================================

    START_FUNC(_interpreterUnresolvedDirectVirtualGlue,intpUnrDVG)

    AlignSB(_intpUnrDirVG_B)
    LR_GPR  rSB,r14                    # set up snippet base register

LABEL(_interpreterUnresolvedDirectVirtualGlue_BODY)

    L_GPR   r1,eq_codeRA_inSnippet(,rSB) # p1) Load code cache RA
    L_GPR   r2,eq_cp_inSnippet(,rSB)     # p2) Load constant pool lit
    XR_GPR  r3,r3
    ICM     r3,7,eq_cpindex_inSnippet+1(rSB) # P3) Load cp index

LOAD_ADDR_FROM_TOC(r14,TR_S390jitResolveSpecialMethod)

    BASR    r14,r14                     # Call jitResolveSpecialMethod

LABEL(LUnresolved_common_code)
ZZ Check to see if this is a <clinit>.  We want to mask the clinit
ZZ bit, to ensure that we have the correct aligned EP.
    LR_GPR  r1,r2                  # put method pointer in R1

    TML     r1,HEX(0001)           # data is masked for clinit?
    JZ      LSave_MethodPtr        # if no, save the method pointer

    NILL    r1,HEX(FFFE)           # else, mask off the lower bit

LABEL(LSave_MethodPtr)
ZZ Save the original method pointer field into the snippet - could
ZZ potentially contain <clinit> bit.
    ST_GPR  r2,eq_methodptr_inSnippet(,rSB)

ifdef([MCC_SUPPORTED],[dnl
ZZ Adjust the trampoline reservations to remove over-reservations
ZZ since this site is resolved now.
ZZ Parameters:
ZZ      callSite, j9Method, cpAddress, cpIndex
ZZ Call to reservationAdjustment
ZZ parameters:  callSite, J9Method, constantPool, constantPoolIndex
ZZ Ptr to return value.
    AHI_GPR J9SP,-5*PTR_SIZE       # Allocate Stack space for params
    ST_GPR  r14,0(J9SP)            # Save the return address
    L_GPR   r2,eq_codeRA_inSnippet(,rSB)
    AHI_GPR r2,-6                  # Address to patch
    ST_GPR  r2,8(J9SP)             # Load Param1: callSite.
    ST_GPR  r1,16(J9SP)            # Load Param2: J9Method
    L_GPR   r2,eq_cp_inSnippet(,rSB)
    ST_GPR  r2,24(J9SP)            # Load Param3: constantPool
    XR_GPR  r2,r2
    ICM     r2,7,eq_cpindex_inSnippet+1(rSB) # P3) Load cp index
    ST_GPR  r2,32(J9SP)            # Load Param4: const. Pool Index
ZZ JitCallCFunction calling convention is:
ZZ r1: C Function Ptr
ZZ r2: Ptr to parameters
ZZ r3: Ptr to Return Address
    LA      r2,8(,J9SP)            # Store pointer to arguments
    LA      r3,0(,J9SP)            # Store pointer to return address
LOAD_ADDR_FROM_TOC(r1,TR_S390mcc_reservationAdjustment_unwrapper)
LOAD_ADDR_FROM_TOC(r14,TR_S390jitCallCFunction)
    BASR    r14,r14                # Call to jitCallCFunction

    L_GPR   r1,16(,J9SP)           # Restore Method Pointers
    L_GPR   r2,eq_methodptr_inSnippet(,rSB)
    L_GPR   r14,0(J9SP)            # Restore Return Address
    AHI_GPR J9SP,5*PTR_SIZE        # Restore stack pointer
])dnl

ZZ Now we want to find the appropriate jit helper method
ZZ to handle this resolved method (i.e. to patch code.)
ZZ We have 3 types of methods:
ZZ    1. Native
ZZ    2. Sync
ZZ    3. Not Sync
ZZ  We want:
ZZ     r1 - the helper glue function to patch the mainline code
ZZ          on next iteration/call of the method.
ZZ     r14 - VM Routine to method. This is the routine that will
ZZ           be called at the end of this glue function.

LABEL(LTest_isNative)

LOAD_ADDR_FROM_TOC(r14,TR_S390jitMethodIsNative)
    BASR    r14,r14                # Call jitMethodIsNative

    LTR_GPR r2,r2                  # test IsNative r2=0 -> not native
    JE      LtestSync              # jump to IsSync test

ZZ Load icallVMprJavaSendNativeStatic
LOAD_ADDR_FROM_TOC(r14,TR_icallVMprJavaSendNativeStatic)

ZZ Load nativeStaticHelper address for patching
LOAD_ADDR_FROM_TOC(r1,TR_S390nativeStaticHelper)

    J       LupdSnippet

LABEL(LtestSync)

LOAD_ADDR_FROM_TOC(r14,TR_S390jitMethodIsSync)
    BASR    r14,r14                        # Call

    XR_GPR  r3,r3
    ICM     r3,1,eq_cpindex_inSnippet(rSB) # Load method table offset
    LTR_GPR r2,r2                   # test IsSync r2=0 -> not sync
    JE      LNotSync

ZZ# Load appropriate SendStaticSync method
LOAD_ADDR_FROM_TOC_IND_REG(r1,TR_S390interpreterSyncVoidStaticGlue,r3)

ZZ# Load appropriate icallVMprJavaSendStatic
LOAD_ADDR_FROM_TOC_IND_REG(r14,TR_icallVMprJavaSendStaticSync0,r3)

    J       LupdSnippet

LABEL(LNotSync)

ZZ Load appropriate _interpreterStaticGlue
LOAD_ADDR_FROM_TOC_IND_REG(r1,TR_S390interpreterVoidStaticGlue,r3)

ZZ Load appropriate icallVMprJavaSendStatic
LOAD_ADDR_FROM_TOC_IND_REG(r14,TR_icallVMprJavaSendStatic0,r3)

LABEL(LupdSnippet)
ZZ Check <clinit> for Patching:
ZZ   We check the <clinit> bit.
ZZ   If <clinit> is NOT set, (i.e. not clinit), then we store the
ZZ   patching method (currently in r1) into the snippet, so on the
ZZ   next iteration/call of this unresolved method, we would jump
ZZ   to the patching routine to fix mainline code.
ZZ   If <clinit> is SET, we don't store this routine into the snippet
ZZ   so, the next iteration will cause it to go through the mainline
ZZ   path.

    L_GPR   r3,eq_methodptr_inSnippet(,rSB) # load method pointer back
    TML     r3,HEX(0001)           # data is masked for clinit?
    JNZ     Linvoke_Method         # if yes, don't update method addr

ZZ update method address in snippet
    ST_GPR  r1,eq_methodaddr_inSnippet(,rSB)

LABEL(Linvoke_Method)
ZZ   Save the resolved entry point as argument 1 for the VM call
ZZ   stored in r14.
    L_GPR   r1,eq_methodptr_inSnippet(,rSB) # get method pointer

ZZ  mask off the low bit in case it is masked for clinit
    NILL    r1,HEX(FFFE)

    LR_GPR  r0,r14

ZZ   Already compiled
    TM      eq_methodCompiledFlagOffset(r1),J9TR_MethodNotCompiledBit
    LR_GPR  r14,rSB
    JZ      Ljitted            # Method is jitted?

ZZ   Go through j2iTransition

    L_GPR   r14,eq_codeRA_inSnippet(rSB)  # Load code cache RA
    LR_GPR  rEP,r0
    BR      rEP

    END_FUNC(_interpreterUnresolvedDirectVirtualGlue,intpUnrDVG,12)


ZZ ===================================================================
ZZ PICBuilder routine _interpreterVoidStaticGlue
ZZ
ZZ ===================================================================

    START_FUNC(_interpreterVoidStaticGlue,intpVStG)

ZZ # align snippet base register
    AlignSB(_intpVStG_B)

LABEL(_interpreterVoidStaticGlue_BODY)

LOAD_ADDR_FROM_TOC(rEP,TR_icallVMprJavaSendStatic0)

    J       LStaticGlueCallFixer

    END_FUNC(_interpreterVoidStaticGlue,intpVStG,10)

ZZ ===================================================================
ZZ PICBuilder routine _interpreterSyncVoidStaticGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterSyncVoidStaticGlue,intpSVStG)

ZZ # align snippet base register
    AlignSB(_intpSynVStG_B)

LABEL(_interpreterSyncVoidStaticGlue_BODY)

LOAD_ADDR_FROM_TOC(rEP,TR_icallVMprJavaSendStaticSync0)

    J       LStaticGlueCallFixer

    END_FUNC(_interpreterSyncVoidStaticGlue,intpSVStG,11)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterIntStaticGlue
ZZ
ZZ ===================================================================

    START_FUNC(_interpreterIntStaticGlue,intpIStG)

ZZ # align snippet base register
    AlignSB(_intpIStG_B)

LABEL(_interpreterIntStaticGlue_BODY)

LOAD_ADDR_FROM_TOC(rEP,TR_icallVMprJavaSendStatic1)

    J       LStaticGlueCallFixer

    END_FUNC(_interpreterIntStaticGlue,intpIStG,10)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterSyncIntStaticGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterSyncIntStaticGlue,intpSIStG)

ZZ # align snippet base register
    AlignSB(_intpSynIStG_B)

LABEL(_interpreterSyncIntStaticGlue_BODY)

LOAD_ADDR_FROM_TOC(rEP,TR_icallVMprJavaSendStaticSync1)

    J       LStaticGlueCallFixer

    END_FUNC(_interpreterSyncIntStaticGlue,intpSIStG,11)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterLongStaticGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterLongStaticGlue,intpLStG)

ZZ # align snippet base register
    AlignSB(_intpLStG_B)

LABEL(_interpreterLongStaticGlue_BODY)

LOAD_ADDR_FROM_TOC(rEP,TR_icallVMprJavaSendStaticSyncJ)

    J       LStaticGlueCallFixer

    END_FUNC(_interpreterLongStaticGlue,intpLStG,10)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterSyncLongStaticGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterSyncLongStaticGlue,intpSLStG)

    AlignSB(_intpSynLStG_B)
ZZ # align snippet base register

LABEL(_interpreterSyncLongStaticGlue_BODY)

LOAD_ADDR_FROM_TOC(rEP,TR_icallVMprJavaSendStaticSyncJ)

    J       LStaticGlueCallFixer

    END_FUNC(_interpreterSyncLongStaticGlue,intpSLStG,11)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterFloatStaticGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterFloatStaticGlue,intpFStG)

ZZ # align snippet base register
    AlignSB(_intpFStG_B)

LABEL(_interpreterFloatStaticGlue_BODY)

LOAD_ADDR_FROM_TOC(rEP,TR_icallVMprJavaSendStaticSyncF)

    J       LStaticGlueCallFixer

    END_FUNC(_interpreterFloatStaticGlue,intpFStG,10)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterSyncFloatStaticGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterSyncFloatStaticGlue,intpSFStG)

ZZ # align snippet base register
    AlignSB(_intpSynFStG_B)

LABEL(_interpreterSyncFloatStaticGlue_BODY)

LOAD_ADDR_FROM_TOC(rEP,TR_icallVMprJavaSendStaticSyncF)

    J       LStaticGlueCallFixer

    END_FUNC(_interpreterSyncFloatStaticGlue,intpSFStG,11)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterDoubleStaticGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterDoubleStaticGlue,intpDStG)

ZZ # align snippet base register
    AlignSB(_intpDStG_B)

LABEL(_interpreterDoubleStaticGlue_BODY)

LOAD_ADDR_FROM_TOC(rEP,TR_icallVMprJavaSendStaticSyncD)

    J       LStaticGlueCallFixer

    END_FUNC(_interpreterDoubleStaticGlue,intpDStG,10)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterSyncDoubleStaticGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterSyncDoubleStaticGlue,intpSDStG)

ZZ # align snippet base register
    AlignSB(_intpSynDStG_B)

LABEL(_interpreterSyncDoubleStaticGlue_BODY)

LOAD_ADDR_FROM_TOC(rEP,TR_icallVMprJavaSendStaticSyncD)


ZZ ===================================================================
ZZ
ZZ  This glue code will be used by _interpreter*StaticGlue methods
ZZ  Before Jumping to this Label,
ZZ  Setup rEP with valid interpreter icallVMprJavaSend*Static*
ZZ
ZZ ===================================================================

LABEL(LStaticGlueCallFixer)

    L_GPR   r1,eq_methodptr_inSnippet(r14) # Load method pointer
    NILL    r1,HEX(FFFE)           # clear <clinit> bit (169312)

    TM      eq_methodCompiledFlagOffset(r1),J9TR_MethodNotCompiledBit
    JZ      Ljitted            # Method is jitted?
    L_GPR   r14,eq_codeRA_inSnippet(r14)  # Load code cache RA
    BR      rEP                # Make the call to interpreter

LABEL(Ljitted)

ifdef([MCC_SUPPORTED],[dnl
ZZ Call MCC service for code patching of the resolved method.
ZZ Parameters:
ZZ       j9method, callSite, newPC, extraArg
ZZ  We have to load the arguments onto the stack to pass to
ZZ  jitCallCFunction:
ZZ         SP + 0:   Save Entry Point (not used by JitCallC)
ZZ         SP + 8:   Return Address.
ZZ         SP +16:   J9Method
ZZ         SP +24:   callSite
ZZ         SP +32:   newPC
ZZ         SP +40:   extraArg (not used in zLinux MCC Patching)
    L_GPR   r1,J9TR_MethodPCStartOffset(r1)  # Load PCStart
    AHI_GPR J9SP,-6*PTR_SIZE       # Allocate Stack space for params
    ST_GPR  r1,0(J9SP)             # Save PCStart for branch at end
    ST_GPR  r14,8(J9SP)            # Save the return address
    L_GPR   r2,eq_methodptr_inSnippet(r14)
    NILL    r2,HEX(FFFE)           # clear <clinit> bit (169312)

    ST_GPR  r2,16(J9SP)            # Load Param1: J9Method.
    L_GPR   r2,eq_codeRA_inSnippet(r14)
    AHI_GPR r2,-6                  # Address to Patch (BRASL 6 bytes)
    ST_GPR  r2,24(J9SP)            # Load Param2: callSite
    ST_GPR  r1,32(J9SP)            # Load Param3: newPC
    ST_GPR  r0,40(J9SP)            # Load Param4: extraArg
ZZ JitCallCFunction calling convention is:
ZZ r1: C Function Ptr
ZZ r2: Ptr to parameters
ZZ r3: Ptr to Return Address
    LA      r2,16(,J9SP)           # Store pointer to arguments
    LA      r3,8(,J9SP)            # Store pointer to return address
LOAD_ADDR_FROM_TOC(r1,TR_S390mcc_callPointPatching_unwrapper)
LOAD_ADDR_FROM_TOC(r14,TR_S390jitCallCFunction)
    BASR    r14,r14                # Call to jitCallCFunction

    L_GPR   rEP,0(J9SP)            # Restore Entry Point
    L_GPR   r14,8(J9SP)            # Restore Return Address.
    AHI_GPR J9SP,6*PTR_SIZE        # Restore Stack Pointer

ZZ This will now proceed to LJitted_end

],[dnl

    L_GPR   r1,J9TR_MethodPCStartOffset(r1)  # Load PCStart
    LR_GPR  rEP,r1        # save PCStart for branch at end

ifdef([TR_HOST_64BIT],[dnl
    LGF     r2,-4(r1)     # Load magic word
],[dnl
    LR_GPR  r2,r1         # Copy PCStart
    AHI_GPR r2,-4         # Move up to the magic word
    L       r2,0(r2)      # Load magic word
])dnl

ZZ                        # The first half of magic word is
ZZ                        # jit-to-jit offset, so we need to
    SRL     r2,16         # shift it to lower half
    AR_GPR  r1,r2                         # Add offset to PCStart

    L_GPR   r2,eq_codeRA_inSnippet(r14)   # Load code RA
    AHI_GPR r2,-4                         # Address to patch

    LR_GPR  r3,r2
    AHI_GPR r3,-2      # start of Instruction to be patched

    CLI     0(r3),HEX(C0)                 # is BRASL?

    JZ      L_BRASL

ZZ  # so it must be L/LG Rx,[0 or R7],R6(D)
ZZ  # For This case, we will get the address
ZZ  # where the snippet address was stored
ZZ  # and patch that. So any thread will either
ZZ  # get the snippet address or
ZZ  # jit code entrypoint ; Data snippets are always aligned
ZZ  # by their length, so ST/STG is ok, don't need to use patch macro

ZZ  # We will prepare the ST/STG instruction and store it in stack
ZZ  # then use EX to execute it for patching the branch address
    AHI_GPR J9SP,-PTR_SIZE

ifdef([TR_HOST_64BIT],[dnl
ZZ  # Put the extended part of STG (i.e. 0x0024) at 4(J9SP)
    LHI     r2,HEX(24)
    SLL     r2,16
    ST      r2,4(J9SP)

    L       r2,0(r3)      # Load the L instruction in r2
    LHI     r3,HEX(E31)   # Load STG 1, part in r3
],[dnl
    L       r2,0(r3)      # Load the L instruction in r2
    LHI     r3,HEX(501)   # Load ST 1, part in r3
])dnl
    SLL     r3,20         # Move ST 1, part at proper position

ZZ  Clean up top 12 bits of L instruction
    SLL     r2,12
    SRL     r2,12

    OR      r2,r3  #r2 now has the first 32 bits of ST/STG
    ST      r2,0(J9SP)    # Store it in stack
    EX      0,0(J9SP)     # Execute it to patch the data snippet

    AHI_GPR J9SP,PTR_SIZE # adjust the stack back
    J       Ljitted_end   # done

LABEL(L_BRASL)
ZZ  # In case of BRASL, we will just patch the offset part
ZZ  # of the BRASL instruction
    SR_GPR  r1,r3            # offset of branch target from BRASL

ifdef([TR_HOST_64BIT],[dnl
    srlg    r1,r1,1          # divide the offset by 2
],[dnl
    srl     r1,1             # divide the offset by 2
])dnl

ZZ  Patch the immediate field of the BRASL
    ST      r1,0(,r2)
])dnl

LABEL(Ljitted_end)
    L_GPR   r14,eq_codeRA_inSnippet(r14)  # Load code cache RA

    BR      rEP  # jump to jitted code from the start to load args

ZZ End of LStaticGlueCallFixer

    END_FUNC(_interpreterSyncDoubleStaticGlue,intpSDStG,11)

ZZ ===================================================================
ZZ  PICBuilder routine - _nativeStaticHelper
ZZ
ZZ ===================================================================

    START_FUNC(_nativeStaticHelper,natStHlpr)

    AlignSB(_natStH_B)
ZZ                # align snippet base register

LABEL(_nativeStaticHelper_BODY)

    L_GPR   r1,eq_methodptr_inSnippet(r14) # Load method pointer
    NILL    r1,HEX(FFFE)           # clear <clinit> bit (169312)

    L_GPR   r14,eq_codeRA_inSnippet(r14)    # Load code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_icallVMprJavaSendNativeStatic)

    BR      rEP                              # Make the call

    END_FUNC(_nativeStaticHelper,natStHlpr,11)

ZZ ===================================================================
ZZ _interpreterUnresolved{Class,String,StaticData,StaticDataStore}Glue
ZZ
ZZ This glue function is called by an unresolved call data snippet.
ZZ It in turn calls a VM routine (i.e. jitResolve{Class,ClassFrom-
ZZ StaticField,StaticField,StaticFieldSetter}) to resolve an
ZZ unresolved class/field, stores the resolved address in the literal
ZZ pool, patches mainline code, and returns to the point
ZZ where the call was originally made in the code cache.
ZZ
ZZ The various versions of the glue function share the same snippet
ZZ patching code, starting at label LDataUnresolved_common_code.
ZZ
ZZ Arguments to the VM Resolution (jitResolve*Method) are stored
ZZ in the snippet code.  The VM resolution routines have three
ZZ parameters:
ZZ                 r1:   constant pool address
ZZ                 r2:   constant pool index
ZZ                 r3:   codeCache RA
ZZ
ZZ For clinit's, we do not patch the main code cache until it has
ZZ be resolved (i.e. LSB is not 1).  This forces all passes of
ZZ of the main code to go through the longer resolve path.
ZZ
ZZ An unresolved data access in main code cache BEFORE RESOLUTION
ZZ would look like:
ZZ             BCRL <UnresolvedDataSnippet> // Branch to PIC gluecode
ZZ             LG   rA, I2(,rLP)    // Load from Literal Pool
ZZ             LG   r2, 0(,rA)      // Load the data.
ZZ
ZZ  The glue code stores the resolved address into I2(,rLP), so
ZZ  that the first load after BCRL will pick up the resolved
ZZ  address properly.  The glue code loads from the snippet
ZZ  the correct address of the literal pool slot to store the address.
ZZ
ZZ An unresolved data access in main code AFTER RESOLUTION
ZZ will look like:
ZZ             BCRL 3               // Branch past itself (NOP)
ZZ             LG   rA, I2(,rLP)    // Load from Literal Pool
ZZ             LG   r2, 0(,rA)      // Load the data.
ZZ
ZZ   Since the address has been resolved and stored into the literal
ZZ   pool, there is no need to "re-resolve" it again.
ZZ
ZZ Note:  Different Branch and Load instructions can be used:
ZZ     Branch: BCRL / BCR
ZZ The gluecode will check the opcodes, and handlexs these variations
ZZ properly.
ZZ ===================================================================
ZZ
ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterUnresolvedClassGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterUnresolvedClassGlue,intpUCG)

    SaveRegs

LABEL(_interpreterUnresolvedClassGlue_BODY)

    L_GPR   r1,eq_cp_inDataSnippet(,r14)      # p1)constant pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14) # p2)cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)  # p3)code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveClass)

    LR_GPR  r0,r14
    BASR    r14,rEP                            # Call jitResolvedClass

    J       LDataResolve_common_code

    END_FUNC(_interpreterUnresolvedClassGlue,intpUCG,9)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterUnresolvedClassGlue2
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterUnresolvedClassGlue2,intpUCG2)

    SaveRegs

LABEL(_interpreterUnresolvedClassGlue2_BODY)

    L_GPR   r1,eq_cp_inDataSnippet(,r14)      # p1) constant pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14) # p2)cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)  # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveClassFromStaticField)

    LR_GPR  r0,r14
    BASR    r14,rEP            # Call jitResolvedClassFromStaticField

    J       LDataResolve_common_code

    END_FUNC(_interpreterUnresolvedClassGlue2,intpUCG2,10)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterUnresolvedStringGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterUnresolvedStringGlue,intpUSG)

    SaveRegs

LABEL(_interpreterUnresolvedStringGlue_BODY)

    L_GPR   r1,eq_cp_inDataSnippet(,r14)      # p1)constant pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14) # p2)cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)  # p3)code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveString)

    LR_GPR  r0,r14
    BASR    r14,rEP                     # Call jitResolvedString

    J       LDataResolve_common_code

    END_FUNC(_interpreterUnresolvedStringGlue,intpUSG,9)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterUnresolvedMethodTypeGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterUnresolvedMethodTypeGlue,intpUMTG)

    SaveRegs

LABEL(_interpreterUnresolvedMethodTypeGlue_BODY)

    L_GPR   r1,eq_cp_inDataSnippet(,r14)      # p1)constant pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14) # p2)cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)  # p3)code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveMethodType)

    LR_GPR  r0,r14
    BASR    r14,rEP                     # Call jitResolvedMethodType

    J       LDataResolve_common_code

    END_FUNC(_interpreterUnresolvedMethodTypeGlue,intpUMTG,10)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterUnresolvedMethodHandleGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterUnresolvedMethodHandleGlue,intpUMHG)

    SaveRegs

LABEL(_interpreterUnresolvedMethodHandleGlue_BODY)

    L_GPR   r1,eq_cp_inDataSnippet(,r14)      # p1)constant pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14) # p2)cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)  # p3)code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveMethodHandle)

    LR_GPR  r0,r14
    BASR    r14,rEP                     # Call jitResolvedMethodHandle

    J       LDataResolve_common_code

    END_FUNC(_interpreterUnresolvedMethodHandleGlue,intpUMHG,10)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterUnresolvedCallSiteTableEntryGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterUnresolvedCallSiteTableEntryGlue,intpUCSG)

    SaveRegs

LABEL(_interpreterUnresolvedCallSiteTableEntryGlue_BODY)

    L_GPR   r1,eq_cp_inDataSnippet(,r14)      # p1)constant pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14) # p2)cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)  # p3)code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveInvokeDynamic)

    LR_GPR  r0,r14
    BASR    r14,rEP  # Call jitResolvedCallSiteTableEntry

    J       LDataResolve_common_code

    END_FUNC(_interpreterUnresolvedCallSiteTableEntryGlue,intpUCSG,10)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterUnresolvedMethodTypeTableEntryGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterUnresolvedMethodTypeTableEntryGlue,intpUMT)

    SaveRegs

LABEL(_interpreterUnresolvedMethodTypeTableEntryGlue_BODY)

    L_GPR   r1,eq_cp_inDataSnippet(,r14)      # p1)constant pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14) # p2)cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)  # p3)code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveHandleMethod)

    LR_GPR  r0,r14
    BASR    r14,rEP  # Call jitResolveHandleMethod

    J       LDataResolve_common_code

    END_FUNC(_interpreterUnresolvedMethodTypeTableEntryGlue,intpUMT,9)

ZZ ===================================================================
ZZ  PICBuilder routine - MTUnresolvedInt32Load
ZZ
ZZ ===================================================================
    START_FUNC(MTUnresolvedInt32Load,intpMTI32L)
ifdef([J9VM_OPT_TENANT],[dnl
    SaveRegs

    L_GPR   r1,eq_cp_inDataSnippet(,r14)         # p1) const pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14)    # p2) cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)     # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveStaticField)

    LR_GPR  r0,r14
    BASR    r14,rEP                      # Call jitResolvedStaticField

    LHI     r1,2                                 # shift for int32
    MTComputeStaticAddress(Int32Load,32)
    MVC     (2*PTR_SIZE+4)(4,r5),0(r2)           # load data to r2 low

    RestoreRegs
    L_GPR   r14,eq_codeRA_inDataSnippet(,r14)    # return to mainline
    BR      r14
])dnl
    END_FUNC(MTUnresolvedInt32Load,intpMTI32L,12)

ZZ ===================================================================
ZZ  PICBuilder routine - MTUnresolvedInt64Load
ZZ
ZZ ===================================================================
    START_FUNC(MTUnresolvedInt64Load,intpMTI64L)
ifdef([J9VM_OPT_TENANT],[dnl
    SaveRegs

    L_GPR   r1,eq_cp_inDataSnippet(,r14)         # p1) const pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14)    # p2) cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)     # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveStaticField)

    LR_GPR  r0,r14
    BASR    r14,rEP                      # Call jitResolvedStaticField

    LHI     r1,3                                 # shift for int64
    MTComputeStaticAddress(Int64Load,64)
    MVC     (2*PTR_SIZE)(8,r5),0(r2)             # load data to r2

    RestoreRegs
    L_GPR   r14,eq_codeRA_inDataSnippet(,r14)    # return to mainline
    BR      r14
])dnl
    END_FUNC(MTUnresolvedInt64Load,intpMTI64L,12)

ZZ ===================================================================
ZZ  PICBuilder routine - MTUnresolvedFloatLoad
ZZ
ZZ ===================================================================
    START_FUNC(MTUnresolvedFloatLoad,intpMTFL)
ifdef([J9VM_OPT_TENANT],[dnl
    SaveRegs

    L_GPR   r1,eq_cp_inDataSnippet(,r14)         # p1) const pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14)    # p2) cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)     # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveStaticField)

    LR_GPR  r0,r14
    BASR    r14,rEP                      # Call jitResolvedStaticField

    LHI     r1,2                                 # shift for float
    MTComputeStaticAddress(FloatLoad,32)
    LE      f0,0(,r2)                            # load the data

    RestoreRegs
    L_GPR   r14,eq_codeRA_inDataSnippet(,r14)    # return to mainline
    BR      r14
])dnl
    END_FUNC(MTUnresolvedFloatLoad,intpMTFL,10)

ZZ ===================================================================
ZZ  PICBuilder routine - MTUnresolvedDoubleLoad
ZZ
ZZ ===================================================================
    START_FUNC(MTUnresolvedDoubleLoad,intpMTDL)
ifdef([J9VM_OPT_TENANT],[dnl
    SaveRegs

    L_GPR   r1,eq_cp_inDataSnippet(,r14)         # p1) const pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14)    # p2) cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)     # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveStaticField)

    LR_GPR  r0,r14
    BASR    r14,rEP                      # Call jitResolvedStaticField

    LHI     r1,3                                 # shift for double
    MTComputeStaticAddress(DoubleLoad,64)
    LD      f0,0(,r2)                            # load the data

    RestoreRegs
    L_GPR   r14,eq_codeRA_inDataSnippet(,r14)    # return to mainline
    BR      r14
])dnl
    END_FUNC(MTUnresolvedDoubleLoad,intpMTDL,10)

ZZ ===================================================================
ZZ  PICBuilder routine - MTUnresolvedAddressLoad
ZZ
ZZ ===================================================================
    START_FUNC(MTUnresolvedAddressLoad,intpMTAL)
ifdef([J9VM_OPT_TENANT],[dnl
    SaveRegs

    L_GPR   r1,eq_cp_inDataSnippet(,r14)         # p1) const pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14)    # p2) cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)     # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveStaticField)

    LR_GPR  r0,r14
    BASR    r14,rEP                      # Call jitResolvedStaticField

ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    LHI     r1,2                                 # shift for addr
],[dnl
    LHI     r1,3                                 # shift for addr
])dnl

    NILL    r2,HEX(FFFE)                      # mask off clinit
    TML     r2,HEX(0002)                      # if not isolated
    JZ      LMTStaticAddressLoadNoneIsolated

    MTComputeStaticAddress(AddressLoad,Obj)
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    LLGF    r3,0(,r2)                        # load compressed val
    SLLG    r3,r3,0(r1)                       # decompress
    STG     r3,(2*PTR_SIZE)(,r5)             # store val into r2
],[dnl
    MVC     (2*PTR_SIZE)(8,r5),0(r2)          # load data to r2
])dnl

    J       LMTStaticAddressLoadBRANCHEND
LABEL(LMTStaticAddressLoadNoneIsolated)
    MVC     (2*PTR_SIZE)(8,r5),0(r2)          # load data to r2
LABEL(LMTStaticAddressLoadBRANCHEND)

    RestoreRegs
    L_GPR   r14,eq_codeRA_inDataSnippet(,r14)    # return to mainline
    BR      r14
])dnl
    END_FUNC(MTUnresolvedAddressLoad,intpMTAL,10)

ZZ ===================================================================
ZZ  PICBuilder routine - MTUnresolvedInt32Store
ZZ
ZZ ===================================================================
    START_FUNC(MTUnresolvedInt32Store,intpMTI32S)
ifdef([J9VM_OPT_TENANT],[dnl
    SaveRegs

    L_GPR   r1,eq_cp_inDataSnippet(,r14)         # p1) const pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14)    # p2) cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)     # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveStaticFieldSetter)

    LR_GPR  r0,r14
    BASR    r14,rEP                      # Call jitResolvedStaticField

    LHI     r1,2                                 # shift for int32
    MTComputeStaticAddress(Int32Store,32)
    MVC     0(4,r2),(PTR_SIZE+4)(r5)             # store r1 low

    RestoreRegs
    L_GPR   r14,eq_codeRA_inDataSnippet(,r14)    # return to mainline
    BR      r14
])dnl
    END_FUNC(MTUnresolvedInt32Store,intpMTI32S,12)

ZZ ===================================================================
ZZ  PICBuilder routine - MTUnresolvedInt64Store
ZZ
ZZ ===================================================================
    START_FUNC(MTUnresolvedInt64Store,intpMTI64S)
ifdef([J9VM_OPT_TENANT],[dnl
    SaveRegs

    L_GPR   r1,eq_cp_inDataSnippet(,r14)         # p1) const pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14)    # p2) cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)     # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveStaticFieldSetter)

    LR_GPR  r0,r14
    BASR    r14,rEP                      # Call jitResolvedStaticField

    LHI     r1,3                                 # shift int64
    MTComputeStaticAddress(Int64Store,64)
    MVC     0(8,r2),(PTR_SIZE)(r5)               # store r1 as data

    RestoreRegs
    L_GPR   r14,eq_codeRA_inDataSnippet(,r14)    # return to mainline
    BR      r14
])dnl
    END_FUNC(MTUnresolvedInt64Store,intpMTI64S,12)

ZZ ===================================================================
ZZ  PICBuilder routine - MTUnresolvedFloatStore
ZZ
ZZ ===================================================================
    START_FUNC(MTUnresolvedFloatStore,intpMTFS)
ifdef([J9VM_OPT_TENANT],[dnl
    SaveRegs

    L_GPR   r1,eq_cp_inDataSnippet(,r14)         # p1) const pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14)    # p2) cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)     # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveStaticFieldSetter)

    LR_GPR  r0,r14
    BASR    r14,rEP                      # Call jitResolvedStaticField

    LHI     r1,2                                 # shift for float
    MTComputeStaticAddress(FloatStore,32)
    STE     f0,0(,r2)                            # store the data

    RestoreRegs
    L_GPR   r14,eq_codeRA_inDataSnippet(,r14)    # return to mainline
    BR      r14
])dnl
    END_FUNC(MTUnresolvedFloatStore,intpMTFS,10)

ZZ ===================================================================
ZZ  PICBuilder routine - MTUnresolvedDoubleStore
ZZ
ZZ ===================================================================
    START_FUNC(MTUnresolvedDoubleStore,intpMTDS)
ifdef([J9VM_OPT_TENANT],[dnl
    SaveRegs

    L_GPR   r1,eq_cp_inDataSnippet(,r14)         # p1) const pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14)    # p2) cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)     # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveStaticFieldSetter)

    LR_GPR  r0,r14
    BASR    r14,rEP                      # Call jitResolvedStaticField

    LHI     r1,3                                 # shift for double
    MTComputeStaticAddress(DoubleStore,64)
    STD     f0,0(,r2)                            # store the data

    RestoreRegs
    L_GPR   r14,eq_codeRA_inDataSnippet(,r14)    # return to mainline
    BR      r14
])dnl
    END_FUNC(MTUnresolvedDoubleStore,intpMTDS,10)

ZZ ===================================================================
ZZ  PICBuilder routine - MTUnresolvedAddressStore
ZZ
ZZ ===================================================================
    START_FUNC(MTUnresolvedAddressStore,intpMTAS)
ifdef([J9VM_OPT_TENANT],[dnl
    SaveRegs
    L_GPR   r1,eq_cp_inDataSnippet(,r14)         # p1) const pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14)    # p2) cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)     # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveStaticFieldSetter)

    LR_GPR  r0,r14
    BASR    r14,rEP                      # Call jitResolvedStaticField

    NILL    r2,HEX(FFFE)              # mask off clinit
    TML     r2,HEX(0002)              # if not isolated
    JZ      LMTUnresolvedAddressStoreNonIsolated

    LG      r3,J9TR_VMThread_tenantDataObj(,r13)
    NILL    r2,HEX(FFFC)              # chop flags from address
    LG      r2,0(,r2)                 # load tenant data index
    LLGFR   r2,r2                     # clear high word
    SRLG    r6,r2,16                  # get row index
    NILH    r2,0                      # get col index

    LGR     r14,r0                    # restore r14
    LG      r1,28(,r14)               # load CP word
    LG      r7,(PTR_SIZE)(,r5)        # load JIT r1, obj to store

ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    SLLG    r2,r2,2                   # col offset, data size
    SLLG    r6,r6,2                   # row offset, compressed
    LLGF    r3,8(r6,r3)               # tenantData[row]
    SRLG    r10,r1,8                  # shift amount
    SLLG    r3,r3,0(r10)             # decompress
    LA      r2,8(r2,r3)               # tenantData[row][col]
    SRLG    r7,r7,0(r10)             # compress
    ST      r7,0(,r2)                 # store obj
],[dnl
    SLLG    r2,r2,3                   # col offset, data size
    SLLG    r6,r6,3                   # row offset, 64-bit data
    LG      r3,16(r6,r3)              # tenantData[row]
    LA      r2,16(r2,r3)              # tenantData[row][col]
    STG     r7,0(,r2)                 # store obj
])dnl

    TML     r1,HEX(0001)              # is owning obj required?
    JZ      LMTComputeStaticAddressAddressStoreEnd
    STG     r3,(2*PTR_SIZE)(,r5)      # JIT r2 = owning obj

LABEL(LMTComputeStaticAddressAddressStoreEnd)
    RestoreRegs
    L_GPR   r14,eq_codeRA_inDataSnippet(,r14)    # return to mainline
    BR      r14

LABEL(LMTUnresolvedAddressStoreNonIsolated)
    MVC     0(8,r2),(PTR_SIZE)(r5)    # store JIT r1 as obj

    RestoreRegs
    L_GPR   r14,eq_codeRA_inDataSnippet(,r14)    # return to mainline
    BR      r14
])dnl

    END_FUNC(MTUnresolvedAddressStore,intpMTAS,10)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterUnresolvedStaticDataGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterUnresolvedStaticDataGlue,intpUStDG)

    SaveRegs

LABEL(_interpreterUnresolvedStaticDataGlue_BODY)

    L_GPR   r1,eq_cp_inDataSnippet(,r14)      # p1) const pool literal
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14) # p2)cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)  # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveStaticField)

    LR_GPR  r0,r14
    BASR    r14,rEP                 # Call jitResolvedStaticField

    J       LDataResolve_common_code

    END_FUNC(_interpreterUnresolvedStaticDataGlue,intpUStDG,11)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterUnresolvedStaticDataStoreGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterUnresolvedStaticDataStoreGlue,intpUStDSG)

    SaveRegs

LABEL(_interpreterUnresolvedStaticDataStoreGlue_BODY)

    L_GPR   r1,eq_cp_inDataSnippet(,r14)         # p1) const pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14)    # p2) cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)     # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveStaticFieldSetter)

    LR_GPR  r0,r14
    BASR    r14,rEP       # Call jitResolvedStaticFieldSetter


ZZ This code is used by _interpreterUnresolved*Glue methods
ZZ Before Jumping here from those methods,
ZZ

LABEL(LDataResolve_common_code)

ZZ  # derive base pointer into table
    BRAS    breg,LDataResolve_common_code_instructions

ZZ  First constant is a BRC 3 (jump by 6 bytes)
ZZ  This is used to patch the original branch to
ZZ  Snippet instruction, if the original branch
ZZ  was not a BRCL (BRCL has its displacement patched
ZZ  manually).  See comment for Unresolved*Data*Glue
ZZ  for reason for this branch.
    CONST_4BYTE(A7F40003)

LABEL(LDataResolve_common_code_instructions)
    LR_GPR  r14,r0        # restore r14

ZZ  If this was a <clinit>, we want to remove the bit flag
ZZ  at the end, otherwise, we would be loading off the wrong
ZZ  and unaligned address!
LABEL(LPatchData)
    TML     r2,HEX(0001)    # data is masked for clinit?
    JZ      LPatchLitPool   # if no, jump accordingly
    LR_GPR  r0,r2           # save the resolved data in r0
    NILL    r2,HEX(FFFE)    # mask off the lower bit

ZZ  At this point, we have:
ZZ       r0: original resolved address of data (potentially with
ZZ                1 in LSB for <clinit>
ZZ       r2: the aligned resolved address of the data.
ZZ  What we want to do now is to store the resolved data
ZZ  address into the lit. pool location where it will be loaded
ZZ  by mainline code.
ZZ
LABEL(LPatchLitPool)                        # patch data in lit pool
ZZ load  the litpool address from java stack
    L_GPR   r1,eq_literalPoolAddr_inDataSnippet(r14)
    ST_GPR  r2,0(,r1)                       # store the result
    L       r1,0(,breg)                     # patch value to the call
    L_GPR   r2,eq_codeRA_inDataSnippet(,r14)  # Load RA location
    AHI_GPR r2,-6                   # location of the branch to patch

ZZ  If interpreter tells us it is <clinit>, we do not patch the
ZZ  mainline code ==> we always go through this resolve slowpath
ZZ  until <clinit> bit is unset.
    TML     r0,HEX(0001)        # data is masked for clinit?
    JNZ     L_DataResolveExit   #if yes, goto to exit

ZZ  Patching
ZZ  We now patch the branch to snippet into effectively
ZZ  a nop (branch past itself), since the literal pool contains
ZZ  the resolved data now.
ZZ  If it is BRCL, we simply store the 3 (6 byte instruction/2)
ZZ  into the displacement.
ZZ  If not BRCL, we patch the instruction with BRC 3.
    CLI     0(r2),HEX(C0)                 # is BRCL?
    JZ      L_PatchBRCL1

    PATCH(Lpatch)
    J       L_DataResolveExit

LABEL(L_PatchBRCL1)
    LHI     r1,-16380  # HEX(C004) - BRCL 0x0
    STH     r1,0(,r2)

LABEL(L_DataResolveExit)
    RestoreRegs

ZZ Now we jump back to mainline code.
    L_GPR   r14,eq_codeRA_inDataSnippet(,r14)

    BR      r14

    END_FUNC(_interpreterUnresolvedStaticDataStoreGlue,intpUStDSG,12)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterUnresolvedInstanceDataGlue
ZZ
ZZ ===================================================================

    START_FUNC(_interpreterUnresolvedInstanceDataGlue,intpUIDG)

    SaveRegs

LABEL(_interpreterUnresolvedInstanceDataGlue_BODY)

    L_GPR   r1,eq_cp_inDataSnippet(,r14)      # p1) constant pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14) # p2) cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)  # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveField)

    LR_GPR  r0,r14

    BASR    r14,rEP                 # Call jitResolvedResolveField

    J       LDataResolve_common_code2

    END_FUNC(_interpreterUnresolvedInstanceDataGlue,intpUIDG,10)

ZZ ===================================================================
ZZ  PICBuilder routine - _interpreterUnresolvedInstanceDataStoreGlue
ZZ
ZZ ===================================================================
    START_FUNC(_interpreterUnresolvedInstanceDataStoreGlue,intpUIDSG)

    SaveRegs

LABEL(_interpreterUnresolvedInstanceDataStoreGlue_BODY)

    L_GPR   r1,eq_cp_inDataSnippet(,r14)      # p1) constant pool lit
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14) # p2) cp index
    L_GPR   r3,eq_codeRA_inDataSnippet(,r14)  # p3) code cache RA

LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveFieldSetter)

    LR_GPR  r0,r14

    BASR    r14,rEP           # Call jitResolvedFieldSetter

LABEL(LDataResolve_common_code2)

ZZ  # derive base pointer into table
    BRAS    breg,LDataResolve_common_code2_instructions

    CONST_4BYTE(A7F40003)
    CONST_4BYTE(FFFFF000)
    CONST_4BYTE(FFFF0FFF)
    CONST_4BYTE(0000E000)

ZZ  R2 contains the resolved offset.
LABEL(LDataResolve_common_code2_instructions)

    LR_GPR  r14,r0


ZZ  If resolved offset is greater than 4k
    CHI    r2,HEX(FFF)               # Long Disp if >= 4k
    JH     LDataOOL

    L_GPR   r3,eq_codeRef_inDataSnippet(,r14) #DataRef instr location
ZZ  needsToBeSignExtendedTo8Bytes
ifdef([TR_HOST_64BIT],[dnl
    CLI     0(r3),HEX(59)  #check if this is OC_C
    JNZ     LclearD2
    AHI     r2,4 # integer is in the lower half of the 8 byte slot
])dnl
LABEL(LclearD2)
    L       r1,0(,r3)                # Data Ref. Instruction
    N       r1,4(,breg)              # Zero out the displacement field
    AR      r1,r2                    # Update the displacement field
    ST      r1,0(,r3)                # Store the result
    L       r1,0(,breg)              # Patch value
    L_GPR   r2,eq_codeRA_inDataSnippet(,r14)   # Load RA location
    AHI_GPR r2,-6

ZZ  Patching
    CLI     0(r2),HEX(C0)                 # is BRCL?
    JZ      L_PatchBRCL

    PATCH(LDRcc2i)
    J       L_LDRcc2Exit

ZZ  Patch the immediate field of the BRCL
LABEL(L_PatchBRCL)
    LHI     r1,3
    ST      r1,2(,r2)

LABEL(L_LDRcc2Exit)
    RestoreRegs

    L_GPR r14,eq_codeRA_inDataSnippet(,r14)
    BR      r14

LABEL(LDataOOL)
ZZ  1.  Store the offset into the snippet
    L_GPR   r3,eq_offsetSlot_inDataSnippet(,r14)  # Address of slot
    ST      r2,0(,r3)                         # Store 32-bit offset
ZZ  2.  Patch the data reference instruction
    L_GPR   r3,eq_codeRef_inDataSnippet(,r14) #DataRef instr loc
    L       r1,0(,r3)
    N       r1,8(,breg)                 # Set base reg to R14
    O       r1,12(,breg)                #
    ST      r1,0(,r3)
ZZ  3.  Patch the immediate field of the BRCL
    L_GPR   r2,eq_codeRA_inDataSnippet(,r14)  # Load RA location
    AHI_GPR r2,-6
    L       r1,eq_patchOffset_inDataSnippet(,r14)
    ST      r1,2(,r2)

LABEL(LDataOOLExit)
    RestoreRegs
    LA    r14,eq_outOfLineStart_inDataSnippet(,r14)
    BR    r14

    END_FUNC(_interpreterUnresolvedInstanceDataStoreGlue,intpUIDSG,11)

ZZ ===================================================================
ZZ  PICBuilder routine - _virtualUnresolvedHelper
ZZ  Handles unresolved virtual and unresolved nestmate private methods
ZZ
ZZ  For unresolved virtual call targets, this routine calls VM helper
ZZ  to resolve and patches the JIT'ed code with its VFT offset. This
ZZ  function runs only once in this case.
ZZ
ZZ  For unresolved nestmate private methods, this routine does
ZZ  not do any patching.
ZZ ===================================================================
    START_FUNC(_virtualUnresolvedHelper,virUH)

    AHI_GPR J9SP,-(3*PTR_SIZE)
    ST_GPR  r3,0(J9SP)
    ST_GPR  r2,PTR_SIZE(J9SP)
    ST_GPR  r1,(2*PTR_SIZE)(J9SP)

ZZ  # derive base pointer into table
    BRAS    rEP,_virtualUnresolvedHelper_BODY

LABEL(_virtualUnresolvedHelper_CONST)

    CONST_4BYTE(A7F40003) # patching instruction
    CONST_4BYTE(FFFF8000) # Max patchable offset

LABEL(_virtualUnresolvedHelper_BODY)
ZZ  Check if it's a resolved private
    L_GPR   r2,eq_privMethod_inVUCallSnippet(r14)
    CHI_GPR r2,0
    JNE     L_privateMethodRemoveTag
ZZ  # Load address of [idx:CP] pair and RA
    LA      r1,eq_cp_inVUCallSnippet(,r14)
    L_GPR   r2,eq_codeRA_inVUCallSnippet(,r14) # Load code cache RA
    LR_GPR  r0,r14

ZZ  # Load jitResolveVirtualMethod address

LOAD_ADDR_FROM_TOC(r14,TR_S390jitResolveVirtualMethod)

    BASR    r14,r14     # Call to resolution

    LR_GPR  r14,r0
ZZ  Check if it's a private method
    LR      r1,r2
    NILL    r1,J9TR_J9_VTABLE_INDEX_DIRECT_METHOD_FLAG
    JNZ     L_privateMethod
ZZ  Skip patching if vft offset is smaller than -32768
    C       r2,4(rEP)
    JL      L_endPatch
LABEL(LVirtualDispatch)

    L_GPR   r3,eq_patchVftInstr_inVUCallSnippet(r14)
    XR      r1,r1      # clear r1
    ICM     r1,1,0(r3) # first opcode byte for LY/LG is E3
    CHI     r1,HEX(E3) # is LY/LG?
    JZ      L_isLY
    STH     r2,2(r3)    # Update the vft offset for A[G]HI
    STH     r2,6(r3)    # Update the vft offset for L[G]HI
    J       L_patchBRASL
LABEL(L_isLY)
    STH     r2,8(r3)    # Update the vft offset field for L[G]HI
ZZ Long Displacement patching for LY/LG
    LR      r1,r2
    SLL     r1,20
    SRL     r1,12
    SLL     r2,12
    SRL     r2,24
    OR      r2,r1
    SLL     r2,8
    O       r2,2(r3)    # skip first 2 bytes of LY/LG
    ST      r2,2(r3)    # update the displacement field

ZZ  Patch the main code cache, as vft offset is now set
LABEL(L_patchBRASL)

    L_GPR   r3,eq_codeRA_inVUCallSnippet(r14)   # Load code cache RA
    AHI_GPR r3,-6    # Get the address in main code cache for patching
    LHI     r1,4
    STC     r1,1(,r3) #this will turn BRASL into a NOP BRCL
    J       L_endPatch

ZZ  -------------------------------------------------------------
ZZ  Private virtual method handling
ZZ
ZZ  Private methods don't get patched
ZZ  They are invoked either directly or via J2I. After they are done,
ZZ  return to mainline execution where the private RA points to.
LABEL(L_privateMethod)
    ST_GPR  r2,eq_privMethod_inVUCallSnippet(r14)
LABEL(L_privateMethodRemoveTag)

ZZ  Load bitwise NOT of the direct method flag
ifdef([J9ZOS390],[dnl
ZZ zOS
    LHI_GPR r0,J9TR_J9_VTABLE_INDEX_DIRECT_METHOD_FLAG
    LCR_GPR r0,r0
    AHI_GPR r0,-1
],[dnl
ZZ zLinux
    LHI_GPR r0,~J9TR_J9_VTABLE_INDEX_DIRECT_METHOD_FLAG
])dnl

    LR_GPR  r1,r2
    NR_GPR  r1,r0          # Remove low-tag of J9Method ptr
    LR_GPR  r3,r14         # free up r14 for RA
    L_GPR   r14,eq_privateRA_inVUCallSnippet(r14) #load RA
    TM      eq_methodCompiledFlagOffset(r1),J9TR_MethodNotCompiledBit
    JZ      L_jitted_private
    LR_GPR  r0,r2         # low-tagged J9Method ptr in R0
    L_GPR   rEP,eq_j2i_thunk_inVUCallSnippet(r3) # load J2I thunk
    J       L_callPrivate_inVU

ZZ  r1 contains J9Method pointer here.
ZZ  Load jitted code entry
LABEL(L_jitted_private)
    L_GPR   r1,J9TR_MethodPCStartOffset(r1)

ifdef([TR_HOST_64BIT],[dnl
    LGF     r2,-4(r1)     # Load reservedWord
],[dnl
    LR_GPR  r2,r1         # Copy PCStart
    AHI_GPR r2,-4         # Move up to the reservedWord
    L       r2,0(r2)      # Load reservedWord
])dnl

ZZ The first half of the reserved word is jit-to-jit offset,
ZZ so we need to shift it to lower half
ZZ see use of getReservedWord() in getJitEntryOffset()
    SRL     r2,16
    AR_GPR  r1,r2        # Add offset to PCStart
    LR_GPR  rEP,r1

LABEL(L_callPrivate_inVU)
    L_GPR   r1,(2*PTR_SIZE)(,J9SP)       # Restore R1
    L_GPR   r2,PTR_SIZE(,J9SP)           # Restore R2
    L_GPR   r3,0(,J9SP)                  # Restore R3
    AHI_GPR J9SP,(3*PTR_SIZE)            # Restore JSP
    BR      rEP       # call private target. Does not return to here
ZZ
ZZ            end of private direct dispatch
ZZ  ---------------------------------------------------------------

LABEL(L_endPatch)
    L_GPR   r1,(2*PTR_SIZE)(,J9SP)       # Restore argument registers
    L_GPR   r2,PTR_SIZE(,J9SP)
    L_GPR   r3,0(,J9SP)
    AHI_GPR J9SP,(3*PTR_SIZE)

LABEL(L_virtualDispatchExit)
    L_GPR   r14,eq_codeRA_inVUCallSnippet(,r14) # Load code cache RA
    BR      r14                # Return

    END_FUNC(_virtualUnresolvedHelper,virUH,7)

ZZ ================================================
ZZ  PICBuilderRoutine : _jitResolveConstantDynamic
ZZ ================================================
    START_FUNC(_jitResolveConstantDynamic,jRCD)

LABEL(_jitResolveConstantDynamicBody)
ZZ Store all the Regsiter on Java Stack and adjust J9SP.
ZZ VM Helper old_slow_jitResolveConstantDynamic allocates
ZZ Data resolve frame for unresolved field and these frame
ZZ assumes all registers are stored off to the Java Stack
ZZ upon entry and adjusts stack frames accordingly during
ZZ stack walk.
   SaveRegs
ZZ Loading arguments for JIT helper
ZZ R1 - Address of Constant Pool
ZZ R2 - CPIndex
ZZ R3 - JIT'd code Return Address
    L_GPR r1,eq_cp_inDataSnippet(,r14)
    LGF_GPR r2,eq_cpindex_inDataSnippet(,r14)
    L_GPR r3,eq_codeRA_inDataSnippet(,r14)

ZZ Following snippet prepares JIT helper call sequence
ZZ which needs r14 to be free which holds address of
ZZ Snippet from where this is called. 
ZZ As jitResolveConstantDynamic is called via 
ZZ SLOW_PATH_ONLY_HELPER glue, it is guaranteed that
ZZ all the registers (volatile and non-volatile) are preserved.
ZZ This allows us to use R0 to preserve R14 
LOAD_ADDR_FROM_TOC(rEP,TR_S390jitResolveConstantDynamic)
    LR_GPR r0,r14
    BASR r14,rEP
    LR_GPR r14,r0

ZZ Load the address of literal pool
ZZ Store the return value of helper call which
ZZ is address of resolved constant dynamic 
ZZ into the Literal Pool 
    L_GPR   r1,eq_literalPoolAddr_inDataSnippet(r14)
    ST_GPR  r2,0(,r1)
    RestoreRegs
    L_GPR r14,eq_codeRA_inDataSnippet(,r14) # Get mainline RA

ZZ Branch instruction in mainline will be patched here with NOP
    MVIY -5(r14),HEX(04)
    BR r14 # Return
    END_FUNC(_jitResolveConstantDynamic,jRCD,6)

ZZ ===================================================================
ZZ  PICBuilder routine - _interfaceCallHelper
ZZ
ZZ ===================================================================
    START_FUNC(_interfaceCallHelper,ifCH)

LABEL(_interfaceCallHelper_BODY)

    AHI_GPR J9SP,-(3*PTR_SIZE)
    ST_GPR  r3,0(J9SP)
    ST_GPR  r2,PTR_SIZE(J9SP)
    ST_GPR  r1,(2*PTR_SIZE)(J9SP)
    LR_GPR  r0,r14


ZZ  Check if it's a previously resolved private target
    L_GPR   r1,eq_intfMethodIndex_inInterfaceSnippet(r14)
    NILL    r1,J9TR_J9_ITABLE_OFFSET_DIRECT
    JZ      ifCH0callResolve
    L_GPR   r1,eq_intfAddr_inInterfaceSnippet(r14)
    CHI_GPR r1,0
    JNZ     ifCHMLTypeCheckIFCPrivate

LABEL(ifCH0callResolve)
    TM      eq_flag_inInterfaceSnippet(r14),1 # method is resolved?
    JNZ     LcontinueLookup

ZZ    # Load address of [idx:CP] pair
    LA      r1,eq_cp_inInterfaceSnippet(,r14)
    L_GPR   r2,eq_codeRA_inInterfaceSnippet(,r14) # Load code cache RA

LOAD_ADDR_FROM_TOC(r14,TR_S390jitResolveInterfaceMethod)

    BASR    r14,r14               # Call to resolution and return

    LR_GPR  r14,r0
ZZ                                # interface class and index in TLS
    MVI     eq_flag_inInterfaceSnippet(r14),1

ZZ  resolve helper fills the class slot and method slot
ZZ  Check PIC slot to see if the target is private interface method
    L_GPR   r1,eq_intfMethodIndex_inInterfaceSnippet(r14)
    NILL    r1,J9TR_J9_ITABLE_OFFSET_DIRECT
    JNZ     ifCHMLTypeCheckIFCPrivate

LABEL(LcontinueLookup)

    L_GPR   r3,eq_codeRA_inInterfaceSnippet(,r14) #Load code cache RA

ZZ  # Load address of interface table & slot number
    LA      r2,eq_intfAddr_inInterfaceSnippet(,r14)
    L_GPR   r1,(2*PTR_SIZE)(,J9SP)  # Load this
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    L       r1,J9TR_J9Object_class(,r1)
ZZ # Load lookup class offset
    LLGFR   r1,r1
],[dnl
    L_GPR   r1,J9TR_J9Object_class(,r1)
ZZ # Load lookup class
])dnl
    NILL    r1,HEX(10000)-J9TR_RequiredClassAlignment

LOAD_ADDR_FROM_TOC(r14,TR_S390jitLookupInterfaceMethod)

    BASR    r14,r14                 # Call jitLookupInterfaceMethod

    LR_GPR  r14,r0
ZZ                                  # return interpVtable offset in r2
    L_GPR   r1,(2*PTR_SIZE)(,J9SP)  # Load this
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    L       r3,J9TR_J9Object_class(,r1)
ZZ # Load offset of the lookup class
    LLGFR   r3,r3
],[dnl
    L_GPR   r3,J9TR_J9Object_class(,r1)
ZZ # Load address of the lookup class
])dnl
    NILL    r3,HEX(10000)-J9TR_RequiredClassAlignment
    L_GPR   r3,0(r2,r3)             # Load method pointer
    L_GPR   r14,eq_codeRA_inInterfaceSnippet(,r14) # codecacheRA
ZZ  32bit integer flag occupies 8byte on 64bit, so...

LABEL(LcommonJitDispatch)         # interpVtable offset in r2

    LNR_GPR r2,r2                 # negative the interpVtable offset
    AHI_GPR r2,J9TR_InterpVTableOffset
    LR_GPR  r0,r2                 # J9 requires the offset in R0
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    L       r3,J9TR_J9Object_class(,r1)
ZZ # Load offset of the lookup class
    LLGFR   r3,r3
],[dnl
    L_GPR   r3,J9TR_J9Object_class(,r1)
ZZ # Load address of the lookup class
])dnl
    NILL    r3,HEX(10000)-J9TR_RequiredClassAlignment
    L_GPR   rEP,0(r2,r3)
    L_GPR   r1,(2*PTR_SIZE)(J9SP) # Restore "this"
    L_GPR   r2,PTR_SIZE(J9SP)
    L_GPR   r3,0(J9SP)
    AHI_GPR J9SP,(3*PTR_SIZE)
    BR      rEP                   # Call: does not return here

    END_FUNC(_interfaceCallHelper,ifCH,6)

ZZ ===================================================================
ZZ  PICBuilder routine - _interfaceCallHelperSingleDynamicSlot
ZZ
ZZ ===================================================================
    START_FUNC(_interfaceCallHelperSingleDynamicSlot,ifCH1)

LABEL(_interfaceCallHelperSingleDynamicSlot_BODY)
    AHI_GPR J9SP,-(3*PTR_SIZE)
    ST_GPR  r1,(2*PTR_SIZE)(J9SP) #this pointer
    ST_GPR  r2,PTR_SIZE(J9SP)
    ST_GPR  r3,0(J9SP)
    LR_GPR  r0,r14


ZZ  Check if it's a previously resolved private target
    L_GPR   r1,eq_intfMethodIndex_inInterfaceSnippet(r14)
    NILL    r1,J9TR_J9_ITABLE_OFFSET_DIRECT
    JZ      ifCH1LcallResolve
    L_GPR   r1,eq_intfAddr_inInterfaceSnippet(r14)
    CHI_GPR r1,0
    JNZ     ifCHMLTypeCheckIFCPrivate

LABEL(ifCH1LcallResolve)
ZZ  check if the method is resolved?
    TM      eq_flag_inInterfaceSnippetSingleDynamicSlot(r14),1
    JNZ     ifCH1LcontinueLookup

ZZ    # Load address of [idx:CP] pair
    LA      r1,eq_cp_inInterfaceSnippet(,r14)
    L_GPR   r2,eq_codeRA_inInterfaceSnippet(,r14) # Load code cache RA

LOAD_ADDR_FROM_TOC(r14,TR_S390jitResolveInterfaceMethod)

    BASR    r14,r14             # Call to resolution and return

    LR_GPR  r14,r0
ZZ                              # interface class and index in TLS
    MVI     eq_flag_inInterfaceSnippetSingleDynamicSlot(r14),1

ZZ  resolve helper fills the class slot and method slot
ZZ  Check PIC slot to see if the target is private interface method
    L_GPR   r1,eq_intfMethodIndex_inInterfaceSnippet(r14)
    NILL    r1,J9TR_J9_ITABLE_OFFSET_DIRECT
    JNZ     ifCHMLTypeCheckIFCPrivate

LABEL(ifCH1LcontinueLookup)

    L_GPR   r3,eq_codeRA_inInterfaceSnippet(,r14) #Load code cache RA

ZZ  # Load address of interface table & slot number
    LA      r2,eq_intfAddr_inInterfaceSnippet(,r14)
    L_GPR   r1,(2*PTR_SIZE)(,J9SP)       # Load this
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    L       r1,J9TR_J9Object_class(,r1)
ZZ # Load lookup class offset
    LLGFR   r1,r1
],[dnl
    L_GPR   r1,J9TR_J9Object_class(,r1)
ZZ # Load lookup class
])dnl
    NILL    r1,HEX(10000)-J9TR_RequiredClassAlignment

LOAD_ADDR_FROM_TOC(r14,TR_S390jitLookupInterfaceMethod)

    BASR    r14,r14             # Call jitLookupInterfaceMethod and
    LR_GPR   rEP,r2             # Store the interpVtable offset

ZZ                                # return interpVtable offset in r2
    L_GPR   r1,(2*PTR_SIZE)(,J9SP)       # Load this
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    L       r3,J9TR_J9Object_class(,r1)
ZZ # Load offset of the lookup class
    LLGFR   r3,r3
],[dnl
    L_GPR   r3,J9TR_J9Object_class(,r1)
ZZ # Load the addr of the lookup class
])dnl
    NILL    r3,HEX(10000)-J9TR_RequiredClassAlignment
    L_GPR   r3,0(r2,r3)           # Load method pointer
    TM   eq_methodCompiledFlagOffset(r3),J9TR_MethodNotCompiledBit
    JNZ     ifCH1LcommonJitDispatch

ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    L       r2,J9TR_J9Object_class(,r1)
ZZ # Read class offset
    LLGFR   r2,r2
],[dnl
    L_GPR   r2,J9TR_J9Object_class(r1)
ZZ # Read class
])dnl
    NILL    r2,HEX(10000)-J9TR_RequiredClassAlignment
    L_GPR   r1,J9TR_MethodPCStartOffset(r3)  # Load PCStart
    LR_GPR  r14,r2        # Copy Class

ifdef([TR_HOST_64BIT],[dnl
    LGF     r2,-4(r1)     # Load magic word
],[dnl
    LR_GPR  r2,r1         # Copy PCStart
    AHI_GPR r2,-4         # Move up to the magic word
    L       r2,0(r2)      # Load magic word
])dnl

ZZ                        # The first half of magic word is
ZZ                        # jit-to-jit offset, so we need to
    SRL     r2,16         # shift it to lower half
    AR_GPR  r2,r1         # Add offset to PCStart

    LR_GPR  r3,r2        # jit-to-jit entry of implementer method
    LR_GPR  r2,r14       # implementer class

    LR_GPR  r14,r0
ifdef([TR_HOST_64BIT],[dnl
    STPQ    r2,eq_implementerClass_inInterfaceSnippet(,r14)
],[dnl
    STM     r2,r3,eq_implementerClass_inInterfaceSnippet(r14)
])dnl

ZZ check if this picSite has already been registered,
ZZ if yes, then no need to register again

ZZ

    TM      eq_picreg_inInterfaceSnippetSingleDynamicSlot(r14),1
    JNZ     ifCH1LcommonJitDispatch

ZZ Clobberable volatile regs r1,r2,r3,r14
ZZ Preserve r0,rEP,r5,r6,r7
ZZ For linux r6 and r7 are not used or clobbered, hence not saved
ZZ For zos rEP(r15) is not clobbered, hence not saved

    L       CARG2,J9TR_J9Class_classLoader(r2)

ZZ  for OMR_GC_COMPRESSED_POINTERS
ZZ  may need to convert r2 to J9Class

    L       CARG1,eq_intfAddr_inInterfaceSnippet(,r14)

ZZ  Compare (CARG2) the classloader of interface class
ZZ  with the classloader of target class (CARG1)
ZZ  If they are same, no need to register the pic site

    C_GPR  CARG2,J9TR_J9Class_classLoader(CARG1)
    JZ      ifCH1LcommonJitDispatch

ZZ  Buy stack space and save regs that may be killed by the c call
ZZ  No need to save the ones that we don't care about.


ifdef([J9ZOS390],[dnl
    AHI_GPR J9SP,-(4*PTR_SIZE)
    ST_GPR  r0,(3*PTR_SIZE)(J9SP)
    ST_GPR  r6,(2*PTR_SIZE)(J9SP)
    ST_GPR  r7,PTR_SIZE(J9SP)
    ST_GPR  r12,0(J9SP)
],[dnl
    AHI_GPR J9SP,-(2*PTR_SIZE)
    ST_GPR  r0,(PTR_SIZE)(J9SP)
    ST_GPR  rEP,0(J9SP)
])dnl

    ST_GPR  J9SP,J9TR_VMThread_sp(r13)

    LA      CARG2,eq_implementerClass_inInterfaceSnippet(r14)

ZZ make the call

ifdef([J9ZOS390],[dnl

LOAD_ADDR_FROM_TOC(r6,TR_jitAddPicToPatchOnClassUnload)

RestoreSSP

ifdef([TR_HOST_64BIT],[dnl

ZZ 64 bit zOS
   LMG r5,r6,0(r6)
   BASR r7,r6
   LR   r0,r0

],[dnl

ZZ 31 bit zOS
   LM r5,r6,16(r6)
   L  r12,J9TR_CAA_save_offset(,rSSP)
   BASR r7,r6
   DC   X'4700',Y((LCALLDESCPICREG-(*-8))/8)   * nop desc

])dnl
SaveSSP
],[dnl

LOAD_ADDR_FROM_TOC(r14,TR_jitAddPicToPatchOnClassUnload)

ZZ zLinux case
    BASR    r14,r14
])dnl

ZZ Restore killed regs
    L_GPR  J9SP,J9TR_VMThread_sp(r13)
ifdef([J9ZOS390],[dnl
    L_GPR  r0,(3*PTR_SIZE)(J9SP)
    L_GPR  r6,(2*PTR_SIZE)(J9SP)
    L_GPR  r7,PTR_SIZE(J9SP)
    L_GPR  r12,0(J9SP)
    AHI_GPR J9SP,(4*PTR_SIZE)
],[dnl
    L_GPR  r0,(PTR_SIZE)(J9SP)
    L_GPR  rEP,0(J9SP)
    AHI_GPR J9SP,(2*PTR_SIZE)
])dnl

ZZ restore java sp
    LR_GPR  r14,r0

ZZ Mark the flag that indicates that this picSite
ZZ has been already registered
    MVI     eq_picreg_inInterfaceSnippetSingleDynamicSlot(r14),1

LABEL(ifCH1LcommonJitDispatch)      # interpVtable offset in rEP

    LR_GPR  r14,r0
    L_GPR   r14,eq_codeRA_inInterfaceSnippet(,r14) # Load codecacheRA
    LNR_GPR rEP,rEP                 # negative the interpVtable offset
    AHI_GPR rEP,J9TR_InterpVTableOffset
    LR_GPR  r0,rEP                  # J9 requires the offset in R0
    L_GPR   r1,(2*PTR_SIZE)(J9SP)   # Restore "this"
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    L       r3,J9TR_J9Object_class(,r1)
ZZ # Load offset of lookup class
    LLGFR   r3,r3
],[dnl
    L_GPR   r3,J9TR_J9Object_class(,r1)
ZZ # Load the address of lookup class
])dnl
    NILL    r3,HEX(10000)-J9TR_RequiredClassAlignment
    L_GPR   rEP,0(rEP,r3)
    L_GPR   r2,PTR_SIZE(J9SP)
    L_GPR   r3,0(J9SP)
    AHI_GPR J9SP,(3*PTR_SIZE)
    BR      rEP                     # Call: does not return here

    END_FUNC(_interfaceCallHelperSingleDynamicSlot,ifCH1,7)


ZZ ===================================================================
ZZ  PICBuilder routine - _interfaceCallHelperMultiSlots
ZZ
ZZ ===================================================================
    START_FUNC(_interfaceCallHelperMultiSlots,ifCHM)

LABEL(_interfaceCallHelperMultiSlots_BODY)

    AHI_GPR J9SP,-(3*PTR_SIZE)
    ST_GPR  r1,(2*PTR_SIZE)(J9SP) #this pointer
    ST_GPR  r2,PTR_SIZE(J9SP)
    ST_GPR  r3,0(J9SP)
    LR_GPR  r0,r14

ZZ  Check if it's a previously resolved private target
    L_GPR   r1,eq_intfMethodIndex_inInterfaceSnippet(r14)
    NILL    r1,J9TR_J9_ITABLE_OFFSET_DIRECT
    JZ      ifCMHLcallResolve
    L_GPR   r1,eq_intfAddr_inInterfaceSnippet(r14)
    CHI_GPR r1,0
    JNZ     ifCHMLTypeCheckIFCPrivate

LABEL(ifCMHLcallResolve)
    TM      eq_flag_inInterfaceSnippet(r14),1 # method is resolved?
    JNZ     ifCHMLcontinueLookup

ZZ    # Load address of [idx:CP] pair
    LA      r1,eq_cp_inInterfaceSnippet(,r14)
    L_GPR   r2,eq_codeRA_inInterfaceSnippet(,r14) # Load code cache RA

LOAD_ADDR_FROM_TOC(r14,TR_S390jitResolveInterfaceMethod)

    BASR    r14,r14          # Call to resolution and return

    LR_GPR  r14,r0
ZZ                           # interface class and index in TLS
    MVI     eq_flag_inInterfaceSnippet(r14),1

ZZ  resolve helper fills the class slot and method slot
ZZ  Check PIC slot to see if the target is private interface method
    L_GPR   r1,eq_intfMethodIndex_inInterfaceSnippet(r14)
    NILL    r1,J9TR_J9_ITABLE_OFFSET_DIRECT
    JNZ     ifCHMLTypeCheckIFCPrivate

LABEL(ifCHMLcontinueLookup)

    L_GPR   r3,eq_codeRA_inInterfaceSnippet(,r14) #Load code cache RA

ZZ  # Load address of interface table & slot number
    LA      r2,eq_intfAddr_inInterfaceSnippet(,r14)
    L_GPR   r1,(2*PTR_SIZE)(,J9SP)       # Load this
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    L       r1,J9TR_J9Object_class(,r1)
ZZ # Load offset of lookup class
    LLGFR   r1,r1
],[dnl
    L_GPR   r1,J9TR_J9Object_class(,r1)
ZZ # Load lookup class
])dnl
    NILL    r1,HEX(10000)-J9TR_RequiredClassAlignment

LOAD_ADDR_FROM_TOC(r14,TR_S390jitLookupInterfaceMethod)

    BASR    r14,r14         # Call jitLookupInterfaceMethod and
    LR_GPR  rEP,r2          # copy the returned interpVtable offset

ZZ                          # returned interpVtable offset in r2
    L_GPR   r1,(2*PTR_SIZE)(,J9SP)       # Load this
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    L       r3,J9TR_J9Object_class(,r1)
ZZ # Load offset of the lookup class
    LLGFR   r3,r3
],[dnl
    L_GPR   r3,J9TR_J9Object_class(,r1)
ZZ # Load the address of the lookup class
])dnl
    NILL    r3,HEX(10000)-J9TR_RequiredClassAlignment
    L_GPR   r3,0(r2,r3)     # Load method pointer
    TM      eq_methodCompiledFlagOffset(r3),J9TR_MethodNotCompiledBit
    LR_GPR  r14,r0
    JNZ     ifCHMLcommonJitDispatch

ZZ  #Load receiving object classPtr in R2
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    L       r2,J9TR_J9Object_class(,r1)
ZZ # Read class offset
    LLGFR   r2,r2
],[dnl
    L_GPR   r2,J9TR_J9Object_class(,r1)
])dnl
    NILL    r2,HEX(10000)-J9TR_RequiredClassAlignment

    L_GPR   r1,J9TR_MethodPCStartOffset(r3)
ZZ  #calculate jit-to-jit entry point from PCStart
ifdef([TR_HOST_64BIT],[dnl
    LGF     r14,-4(r1)    # Load magic word
],[dnl
    LR_GPR  r14,r1        # Copy PCStart
    AHI_GPR r14,-4        # Move up to the magic word
    L       r14,0(r14)    # Load magic word
])dnl

ZZ                        # The first half of magic word is
ZZ                        # jit-to-jit offset, so we need to
    SRL     r14,16        # shift it to lower half
    AR_GPR  r14,r1        # Add offset to PCStart

    LR_GPR  r3,r14        # jit-to-jit entry point

    LR_GPR  r14,r0

ZZ  r2 is classPtr and r3 is jit-to-jit entry point

ZZ  if lastCachedSlot == lastSlot, no more slots left to cache,
ZZ  so just dispatch
    L_GPR   r0,eq_lastCachedSlotField_inInterfaceSnippet(r14)
    C_GPR   r0,eq_lastSlotField_inInterfaceSnippet(r14)
    JZ      ifCHMLcommonJitDispatch

ZZ Try to atomically update lastCachedSlot
ZZ value to be stored(r1) beginning with firstSlot
    L_GPR   r0,eq_firstSlotField_inInterfaceSnippet(r14)
    AHI_GPR r0,-2*PTR_SIZE
LABEL(ifCHMCSLoopBegin)
    LR_GPR  r1,r0
    AHI_GPR r1,2*PTR_SIZE    ZZ  r1 will now point to next empty slot

ZZ  if lastCachedSlot == lastSlot, no more slots left to cache,
ZZ  so just dispatch
    C_GPR   r0,eq_lastSlotField_inInterfaceSnippet(r14)
    JZ      ifCHMLcommonJitDispatch

ZZ Try to atomically update lastCachedSlot
LABEL(ifCHMDoSwap)
ZZ update lastCachedSlot with next empty slot
    CS_GPR  r0,r1,eq_lastCachedSlotField_inInterfaceSnippet(r14)
    JZ      ifCHMUpdateCacheSlot     ZZ Got empty slot so update it

ZZ Someone already grabbed this empty slot
ZZ Loop till the winner updates the empty slot
LABEL(ifCHMLoopTillUpdate)
    L_GPR   r0,0(,r1)
    CHI_GPR r0,0
    JZ      ifCHMLoopTillUpdate

ZZ  current slot is now updated,
ZZ  Lets see if it has same classPtr as we are trying to store
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    L       r0,J9TR_J9Object_class(,r1)
ZZ # Read class offset
    LLGFR   r0,r0
    NILL    r0,HEX(10000)-J9TR_RequiredClassAlignment
    CR      r2,r0
],[dnl
    L_GPR   r0,J9TR_J9Object_class(,r1)
    NILL    r0,HEX(10000)-J9TR_RequiredClassAlignment
    CR_GPR  r2,r0
])dnl
    JZ      ifCHMLcommonJitDispatch ZZ same, so skip caching

ZZ different than cached one, lets try to grab next free slot
ZZ before jumping back, r0 should point to the
ZZ slot we contended for last time
    LR_GPR  r0,r1
    J       ifCHMCSLoopBegin

LABEL(ifCHMUpdateCacheSlot)
ZZ store class pointer and method EP in the current empty slot
    ST_GPR  r3,PTR_SIZE(,r1)
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    ST  r2,0(,r1)
],[dnl
    ST_GPR  r2,0(,r1)
])dnl

ZZ Clobberable volatile regs r1,r2,r3,r0
ZZ  Load pic address as second address
    LR_GPR  CARG2,r1

    L_GPR   CARG1,eq_intfAddr_inInterfaceSnippet(,r14)
    L_GPR   r0,J9TR_J9Class_classLoader(CARG1)

ZZ  Load class pointer as first argument
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    L       CARG1,0(CARG2)   # May need to convert offset to J9Class
    LLGFR   CARG1,CARG1
],[dnl
    L_GPR   CARG1,0(CARG2)
])dnl

ZZ  Compare (r0) the classloader of interface class
ZZ  with the classloader of target class (CARG1)
ZZ  If they are same, no need to register the pic site

    C_GPR  r0,J9TR_J9Class_classLoader(CARG1)
    JZ      ifCHMLcommonJitDispatch

ZZ Need to preserve r14,rEP,r5,r6,r7
ZZ For linux r6 and r7 are not used or clobbered, hence not saved
ZZ For zos r14, rEP(r15) is not clobbered, hence not saved

ifdef([J9ZOS390],[dnl
    AHI_GPR J9SP,-(3*PTR_SIZE)
    ST_GPR  r6,(2*PTR_SIZE)(J9SP)
    ST_GPR  r7,PTR_SIZE(J9SP)
    ST_GPR  r12,0(J9SP)
],[dnl
    AHI_GPR J9SP,-(2*PTR_SIZE)
    ST_GPR  rEP,PTR_SIZE(J9SP)
    ST_GPR  r14,0(J9SP)
])dnl

    ST_GPR  J9SP,J9TR_VMThread_sp(r13)

ZZ make the call

RestoreSSP

ifdef([J9ZOS390],[dnl

LOAD_ADDR_FROM_TOC(r6,TR_jitAddPicToPatchOnClassUnload)

ifdef([TR_HOST_64BIT],[dnl

ZZ 64 bit zOS
   LMG r5,r6,0(r6)
   BASR r7,r6
   LR   r0,r0

],[dnl

ZZ 31 bit zOS
   LM r5,r6,16(r6)
   L  r12,J9TR_CAA_save_offset(,rSSP)
   BASR r7,r6
   DC   X'4700',Y((LCALLDESCPICREG-(*-8))/8)   * nop desc

])dnl
SaveSSP
],[dnl


ZZ zLinux case
LOAD_ADDR_FROM_TOC(r14,TR_jitAddPicToPatchOnClassUnload)
    BASR    r14,r14
])dnl

ZZ Restore killed regs
    L_GPR  J9SP,J9TR_VMThread_sp(r13)
ifdef([J9ZOS390],[dnl
    L_GPR  r6,(2*PTR_SIZE)(J9SP)
    L_GPR  r7,PTR_SIZE(J9SP)
    L_GPR  r12,0(J9SP)
    AHI_GPR J9SP,(3*PTR_SIZE)
],[dnl
    L_GPR  rEP,PTR_SIZE(J9SP)
    L_GPR  r14,0(J9SP)
    AHI_GPR J9SP,(2*PTR_SIZE)
])dnl

LABEL(ifCHMLcommonJitDispatch)    # interpVtable offset in rEP

    L_GPR   r14,eq_codeRA_inInterfaceSnippet(,r14) # Load codecacheRA
    LNR_GPR rEP,rEP               # negative the interpVtable offset
    AHI_GPR rEP,J9TR_InterpVTableOffset
    LR_GPR  r0,rEP                # J9 requires the offset in R0
    L_GPR   r1,(2*PTR_SIZE)(J9SP) # Restore "this"
ifdef([OMR_GC_COMPRESSED_POINTERS],[dnl
    L       r3,J9TR_J9Object_class(,r1)
ZZ # Load offset of the lookup class
    LLGFR   r3,r3
],[dnl
    L_GPR   r3,J9TR_J9Object_class(,r1)
ZZ # Load address of the lookup class
])dnl
    NILL    r3,HEX(10000)-J9TR_RequiredClassAlignment
    L_GPR   rEP,0(rEP,r3)
    L_GPR   r2,PTR_SIZE(J9SP)
    L_GPR   r3,0(J9SP)
    AHI_GPR J9SP,(3*PTR_SIZE)
    BR      rEP                   # Call: does not return here

ZZ ------------------------------------------------------------
ZZ      private interface method handling
ZZ
ZZ  1. do type check by calling fast_jitInstanceOf
ZZ  2. if the type check passes, remove low tag from method
ZZ     extract entry and perform direct dispatch.
ZZ     Otherwise, call lookup helper so that it throws
ZZ     an error.
ZZ
ZZ  Note that this is sharing the JIT direct dispatch with
ZZ  virtual private method, which does not return to this routine

ZZ  use a shorter name for this offset
SETVAL(eq_vmThrSSP,J9TR_VMThread_systemStackPointer)

LABEL(ifCHMLTypeCheckIFCPrivate)

ZZ  Call fast_jitInstanceOf
ZZ  with three args: VMthread, object, and castClass.
ZZ
ZZ  The call to this helper follows J9S390CHelperLinkage except
ZZ  that all volatiles are saved here.
ZZ  instance of result is indicated in return reg
ZZ
ZZ  NOTE: fast_jitInstanceOf has different parameter orders on
ZZ        different platforms
ZZ
ZZ  R1-3 have been saved. just need to save r0, r4-15 here.
    LR_GPR  r0,r14
    LR_GPR  CARG1,r13                                  # vmThr
    L_GPR   CARG2,(2*PTR_SIZE)(J9SP)                   # obj
    L_GPR   CARG3,eq_intfAddr_inInterfaceSnippet(r14)  # class

    AHI_GPR J9SP,-(13*PTR_SIZE)    # save r0, and r4-r15
    ST_GPR  r0,0(J9SP)
    STM_GPR r4,r15,PTR_SIZE(J9SP)

ZZ  Now start to call fast_jitInstanceOf as a C function
    ST_GPR  J9SP,J9TR_VMThread_sp(r13)
ifdef([J9ZOS390],[dnl
    L_GPR   rSSP,eq_vmThrSSP(r13)
    XC      eq_vmThrSSP(PTR_SIZE,r13),eq_vmThrSSP(r13)
ifdef([TR_HOST_64BIT],[dnl
ZZ 64 bit zOS. Do nothing.
],[dnl
ZZ 31 bit zOS. See definition of J9TR_CAA_SAVE_OFFSET
    L_GPR  r12,2080(rSSP)
])dnl

])dnl

LOAD_ADDR_FROM_TOC(r14,TR_instanceOf)

    BASR    r14,r14        # call instanceOf

ifdef([J9ZOS390],[dnl
    ST_GPR   rSSP,eq_vmThrSSP(r13)
])dnl

    L_GPR   J9SP,J9TR_VMThread_sp(r13)

ZZ  restore all regs
    L_GPR   r0,0(J9SP)
    LM_GPR  r4,r15,PTR_SIZE(J9SP)
    AHI_GPR J9SP,(13*PTR_SIZE)
    LR_GPR  r14,r0

    CHI_GPR CRINT,1
    JNE     ifCHMLcontinueLookup

LABEL(ifCHMLInvokeIFCPrivate)
ZZ  remove low tag and call
    L_GPR   r0,eq_intfMethodIndex_inInterfaceSnippet(r14)
    LR_GPR  r3,r14         # free r14 for RA
    LR_GPR  r1,r0          # keep low-tagged in r0

ZZ  bitwise NOT the flag
ifdef([J9ZOS390],[dnl
ZZ zOS
    LHI_GPR r2,J9TR_J9_ITABLE_OFFSET_DIRECT
    LCR_GPR r2,r2
    AHI_GPR r2,-1
],[dnl
ZZ zLinux
    LHI_GPR r2,~J9TR_J9_ITABLE_OFFSET_DIRECT
])dnl

    NR_GPR  r1,r2         # Remove low-tag of J9Method ptr
    L_GPR   r14,eq_codeRA_inInterfaceSnippet(r14)    #load RA
    TM      eq_methodCompiledFlagOffset(r1),J9TR_MethodNotCompiledBit
    JZ      L_jitted_private
    L_GPR   rEP,eq_thunk_inInterfaceSnippet(r3)      # load J2I thunk
    J       L_callPrivate_inVU

    END_FUNC(_interfaceCallHelperMultiSlots,ifCHM,7)

ZZ ===================================================================
ZZ Allocates new object out of line.
ZZ Callable by the JIT generated code.
ZZ If we are out of TLH space, we set CC and
ZZ check it when we return from this helper.
ZZ IT DOES NOT INITIALIZE THE HEADER
ZZ
ZZ Parameters:
ZZ
ZZ   class R1
ZZ   object size R2
ZZ
ZZ Returns:
ZZ
ZZ   R3
ZZ ===================================================================
START_FUNC(outlinedNewObject,ONO)
  A_GPR  R2,J9TR_VMThread_heapAlloc(,J9VM_STRUCT)
  CL_GPR R2,J9TR_VMThread_heapTop(,J9VM_STRUCT)
  L_GPR  R3,J9TR_VMThread_heapAlloc(,J9VM_STRUCT)
  BCR    HEX(2),R14
  ST_GPR R2,J9TR_VMThread_heapAlloc(,J9VM_STRUCT)
  BR     R14
END_FUNC(outlinedNewObject,ONO,5)

ZZ ===================================================================
ZZ Allocates new array out of line.
ZZ Callable by the JIT generated code.
ZZ If we are out of TLH space, we set CC and
ZZ check it when we return from this helper.
ZZ IT DOES NOT INITIALIZE THE HEADER
ZZ
ZZ Parameters:
ZZ
ZZ   byte size R3
ZZ   class R1
ZZ   array size R2
ZZ
ZZ Returns:
ZZ
ZZ   R3
ZZ ===================================================================
START_FUNC(outlinedNewArray,ONA)
ZZ Test if size is 0, means arraylets go to VM helpers
  LTR rEP,R2
  BRC     HEX(8),PatchConditionCode
ZZ Test if size is negative or too large
  SRA     rEP,16
  BRC     HEX(6),PatchConditionCode
  LR_GPR  rEP,R3
ZZ Load HeapAlloc and compare it with HeapTop..
  L_GPR   R3,J9TR_VMThread_heapAlloc(,J9VM_STRUCT)
  AR_GPR  rEP,R3
  CL_GPR  rEP,J9TR_VMThread_heapTop(,J9VM_STRUCT)
  BCR     HEX(2),R14
  ST_GPR  rEP,J9TR_VMThread_heapAlloc(,J9VM_STRUCT)
  BR      R14
LABEL(PatchConditionCode)
  LHI_GPR   rEP,16
  LTR_GPR   rEP,rEP
  BR        R14
END_FUNC(outlinedNewArray,ONA,5)

ZZ ===================================================================
ZZ wrapper to call atomic compare and swap of a 4 byte value
ZZ Note--this is called with C linkage!
ZZ CARG1 patch address
ZZ CARG2 old 32 bit Insn
ZZ CARG3 new 32 bit Insn
ZZ ===================================================================
START_FUNC(_CompareAndSwap4,CS4)
  CS  CARG2,CARG3,0(CARG1)
  LHI_GPR CRINT,0
  BCR  HEX(4),CRA
  LHI_GPR CRINT,1
  BR CRA
END_FUNC(_CompareAndSwap4,CS4,5)

ZZ ===================================================================
ZZ wrapper to call atomic store of a 4 byte value
ZZ Note--this is called with C linkage!
ZZ CARG1 patch address
ZZ CARG2 new 32 bit value
ZZ ===================================================================
START_FUNC(_Store4,ST4)
  ST  CARG2,0(CARG1)
  BR  CRA
END_FUNC(_Store4,ST4,5)

ifdef([TR_HOST_64BIT],[dnl
ZZ ===================================================================
ZZ wrapper to call atomic store of a 8 byte value
ZZ Note--this is called with C linkage!
ZZ CARG1 patch address
ZZ CARG2 new 64 bit value
ZZ ===================================================================
START_FUNC(_Store8,ST8)
  STG CARG2,0(CARG1)
  BR  CRA
END_FUNC(_Store8,ST8,5)
])dnl

ZZ ===================================================================
ZZ Returns the offset of the MVS data area structs which contain
ZZ the leap seconds offset required to correct the STCK time on zOS
ZZ ===================================================================
START_FUNC(_getSTCKLSOOffset,_GSTCKLSO)
ifdef([J9ZOS390],[dnl
  LHI CRINT,FLCCVT-PSA
  ST  CRINT,0(CARG1)
  LHI CRINT,CVTEXT2-CVT
  ST  CRINT,4(CARG1)
  LHI CRINT,CVTLSO-CVTXTNT2
  ST  CRINT,8(CARG1)
  b  RETURNOFFSET(CRA)
],[dnl
  br  CRA
])dnl


END_FUNC(_getSTCKLSOOffset,_GSTCKLSO,11)

ZZ ===================================================================
ZZ Wrapper to modify runtime instrumentation controls.
ZZ CARG1 pointer to RI Control Block
ZZ ==================================================================
START_FUNC(_RIMRIC,RIMRIC)
ifdef([J9ZOS390],[dnl
   DC HEX(eb001000)
   DC HEX(0062)
   b  RETURNOFFSET(CRA)
],[dnl
   .long HEX(eb002000)
   .short HEX(0062)
   BR CRA
])dnl
END_FUNC(_RIMRIC,RIMRIC,8)

ZZ ==================================================================
ZZ Wrapper to modify runtime instrumentation controls.
ZZ CARG1 pointer to RI Control Block
ZZ ==================================================================
START_FUNC(_RISTRIC,RISTRIC)
ifdef([J9ZOS390],[dnl
   DC HEX(eb001000)
   DC HEX(0061)
   b  RETURNOFFSET(CRA)
],[dnl
   .long HEX(eb002000)
   .short HEX(0061)
   BR CRA
])dnl
END_FUNC(_RISTRIC,RISTRIC,9)

ZZ ==================================================================
ZZ Wrapper to enable runtime instrumentation.
ZZ ==================================================================
START_FUNC(_RION,RION)
ifdef([J9ZOS390],[dnl
   DC HEX(AA010000)
   b RETURNOFFSET(CRA)
],[dnl
   .long HEX(AA010000)
   BR CRA
])dnl
END_FUNC(_RION,RION,6)

ZZ ==================================================================
ZZ Wrapper to disable runtime instrumentation.
ZZ ==================================================================
START_FUNC(_RIOFF,RIOFF)
ifdef([J9ZOS390],[dnl
   DC HEX(AA030000)
   b RETURNOFFSET(CRA)
],[dnl
   .long HEX(AA030000)
   BR CRA
   ])dnl
END_FUNC(_RIOFF,RIOFF,7)

ZZ ==================================================================
ZZ Wrapper to test runtime instrumentation controls.
ZZ ==================================================================
START_FUNC(_RITRIC,RITRIC)
ifdef([J9ZOS390],[dnl
   LHI CRINT,0
   DC HEX(aa020000)
   BC 8,RETURNOFFSET(CRA)
   LHI CRINT,1
   BC 4,RETURNOFFSET(CRA)
   LHI CRINT,2
   BC 2,RETURNOFFSET(CRA)
   LHI CRINT,3
   b  RETURNOFFSET(CRA)
],[dnl
   .long HEX(aa020000)
   BCR 8,CRA
   LHI CRINT,1
   BCR 4,CRA
   LHI CRINT,2
   BCR 2,CRA
   LHI CRINT,3
   BR CRA
])dnl
END_FUNC(_RITRIC,RITRIC,8)

SETVAL(rdsa,5)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
SETVAL(dsaSize,32*PTR_SIZE)
],[dnl
SETVAL(dsaSize,16*PTR_SIZE)
])dnl

ZZ ===================================================================
ZZ
ZZ call referenceArrayCopy
ZZ Return in R2, if copy success (-1), otherwise how many copied
ZZ Calls VM routine referenceArrayCopy
ZZ
ZZ Parms stored in parms area in C Stack
ZZ 1) VM Thread
ZZ 2) srcObj
ZZ 3) dstObj
ZZ 4) srcAddr
ZZ 5) dstAddr
ZZ 6) num of slots
ZZ 7) VM referenceArrayCopy func desc
ZZ ===================================================================

    START_FUNC(__referenceArrayCopyHelper,_RACP)
    ST_GPR   r14,PTR_SIZE(,rdsa)
    AHI_GPR  rdsa,-dsaSize
    STM_GPR  r0,r15,0(rdsa)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    STMH_GPR r0,r15,64(rdsa)
])dnl
    LR_GPR   r8,rdsa      # save dsa in r8
ifdef([J9ZOS390],[dnl
ifdef([TR_HOST_64BIT],[dnl
    LM_GPR   r1,r3,2176(r4)
    L_GPR    r10,2224(r4) # get vm referenceArrayCopy func desc
    LM_GPR   r5,r6,0(r10)
    BASR     r7,r6        # Call vm function referenceArrayCopy
    lr       r0,r0        # Nop for XPLINK return
],[dnl
    LM_GPR   r1,r3,2112(r4)
    L_GPR    r12,J9TR_CAA_save_offset(,r4) # Restore CAA for 31-bit
    L_GPR    r10,2136(r4)   # get vm referenceArrayCopy func desc
    LM_GPR   r5,r6,16(r10)
    BASR     r7,r6          # Call vm function referenceArrayCopy
    DC       X'4700',Y((LCALLDESCRACP-(*-8))/8)   * nop desc
])dnl
    LR_GPR   r2,r3          # copy return value
],[dnl
ZZ  z/Linux
ifdef([TR_HOST_64BIT],[dnl
ZZ  z/TPF
ifdef([OMRZTPF],[dnl
    LM_GPR   r2,r6,448(r15)   # load parms to call regs
    MVC      448(PTR_SIZE,r15),488(r15)  # copy to linux parm format
    L_GPR    r1,496(r15)   # get function addr
    BASR     r14,r1        # Call vm function referenceArrayCopy
ZZ  R2 contains return value
],[dnl
    LM_GPR   r2,r6,160(r15)   # load parms to call regs
    MVC      160(PTR_SIZE,r15),200(r15)  # copy to linux parm format
    L_GPR    r1,208(r15)   #get function addr
    BASR     r14,r1        # Call vm function referenceArrayCopy
ZZ  R2 contains return value
])dnl
],[dnl
    LM_GPR   r2,r6,96(r15)    # load parms to call regs
    MVC      96(PTR_SIZE,r15),116(r15)  # copy to linux parm format
    L_GPR    r1,120(r15)   #get function addr
    BASR     r14,r1        # Call vm function referenceArrayCopy
ZZ  R2 contains return value
])dnl
])dnl
    LR_GPR   rdsa,r8 #restore dsa from r8
    LM_GPR   r0,r1,0(rdsa)
    LM_GPR   r3,r15,3*PTR_SIZE(rdsa)
ifdef([J9VM_JIT_32BIT_USES64BIT_REGISTERS],[dnl
    LMH_GPR  r0,r15,64(rdsa)
])dnl
    AHI_GPR  rdsa,dsaSize
    L_GPR    r14,PTR_SIZE(,rdsa)
    br r14

    END_FUNC(__referenceArrayCopyHelper,_RACP,7)

ifdef([J9ZOS390],[dnl
ifdef([TR_HOST_64BIT],[dnl

ZZ 64bit XPLINK doesn't need call descriptors

],[dnl

ZZ We will share this call descriptor for all calls to
ZZ jitAddPicToPatchOnClassUnload
ZZ because it is really just a dummy descriptor anyway..

LCALLDESCPICREG      DS    0D           * Dword Boundary
        DC    A(ZifCH1-*)        *
        DC    BL.3'000',BL.5'00000'   * XPLINK Linkage + Returns: void
        DC    BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
ZZ                                      unprototyped call
])dnl

ZZ Call Descriptor for call to referenceArrayCopy
LCALLDESCRACP     DS    0D         * Dword Boundary
        DC    A(Z_RACP-*)          *
        DC    BL.3'000',BL.5'00001'  * XPLINK Linkage + Returns: int
        DC    BL.6'001000',BL.6'000000',BL.6'000000',BL.6'000000'
ZZ                                     unprototyped call

  LTORG
  IHAPSA
  CVT DSECT=YES

    END
])dnl

