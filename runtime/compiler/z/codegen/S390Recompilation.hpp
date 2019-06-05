/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef S390RECOMPILATION_INCL
#define S390RECOMPILATION_INCL

#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "codegen/PreprologueConst.hpp"

class TR_ResolvedMethod;

/** \brief
 *     This class provides support for method recompilation. There are two modes of recompilation supported:
 *
 *     1. Sampling recompilation
 *     2. Counting recompilation
 *
 *     Sampling recompilation snippet is generated as part of the pre-prologue and the counting recompilation snippet
 *     is generated as part of the prologue. The two recompilation mechanisms are mutually exclusive, meaning at most
 *     one recompilation mechanism may be emitted for a particular compiled body.
 *
 *  \details
 *
 *     1. Sampling recompilation
 *
 *     The following sequence of instructions is an excerpt of a sampling recompilation of java/lang/Object.<init>()V
 *     on a 64-bit Java 8 SR5 JVM on Linux on z.
 *
 *     The sampling recompilation snippet can be composed of two sub-snippets. Following the sub-snippets mini-
 *     trampolines (BRC instructions) are emitted which branch to the respective snippets (<ffffffec> and <ffffffe8>).
 *     This is done to simplify the patching logic as it keeps the offsets from the interpreter entry point
 *     (<00000000>) to the mini-trampolines constant. Following the mini-trampolines we emit metadata constants for
 *     patching (<fffffff0>), the body info address (<fffffff4>), and the reserved word (<fffffffc>).
 *
 *     - VM Call Helper Snippet
 *
 *       The VM Call Helper Snippet (Label L0016) is used to revert the execution of the method back to the interpreter.
 *       This may be done for a variety of reasons, such as a user triggered breakpoint within this method (under full
 *       speed debug), a failed preexistence assumption if preexistence was performed on this method, a failed
 *       recompilation of this method, a method redefinition, etc.
 *
 *       As an example let's consider the full speed debug case in which a user places a breakpoint on the entry of our
 *       (JIT compiled) method (java/lang/Object.<init>()V). Upon registering the breakpoint the VM calls a JIT hook
 *       (_fsdSwitchToInterpPatchEntry) which patches the JIT entry point (<00000006>) with a branch back to the mini-
 *       trampoline which will execute the VM Call Helper Snippet. Before doing so however the JIT hook must preserve
 *       the original bytes at the JIT entry point so that if the user wishes to disable the breakpoint we can happily
 *       continue to execute this compiled body. To achieve this the JIT hook will preserve the original 4-bytes at the
 *       JIT entry point into the save location (<fffffff0>). Now all executions of this JIT compiled body will end up
 *       reverting the execution back to the interpreter until the user disables the breakpoint and another JIT hook
 *       is called (_fsdRestoreToJITPatchEntry) to reverse the patching.
 *
 *     - SamplingRecompileMethod Snippet
 *
 *       The SamplingRecompileMethod Snippet (Label L0017) is used to induce a sampling method recompilation.
 *
 *       As an example let's consider the case in which our (JIT compiled) method (java/lang/Object.<init>()V) has
 *       been sampled enough times by the JIT sampling thread to warrant a recompilation at a higher optimization
 *       level. The sampling thread then calls J9::Recompilation::fixUpMethodCode which patches the JIT entry point
 *       (<00000006>) of the compiled body with a branch back to the mini-trampoline which will execute the
 *       SamplingRecompileMethod Snippet. At runtime when the method is executed the SamplingRecompileMethod Snippet
 *       is executed which branches off to a JIT helper (<ffffffde>) which will induce a recompilation of our method
 *       asynchronously by placing it on the method compilation queue. The compilation thread (re)compiles the method
 *       at the new optimization level and calls J9::Recompilation::methodHasBeenRecompiled. This function then patches
 *       the JIT helper (<ffffffde>) address to point to another JIT helper (_samplingPatchCallSite) which is
 *       responsible for patching (at runtime) the caller of our old method body with a call to the new recompiled
 *       method body. The old method code cache can then be reclaimed but the pre-prologue must be kept intact because
 *       there could still be outstanding callers whose call instructions have not been patched to point to the now
 *       recompiled method body.
 *
 *     * Listed below is an illustration of several defined constants used for pre-prologue patching.
 *
 *     * NOP instructions may be inserted following the VM Call Helper Snippet (<ffffffbc>) and following the
 *       SamplingRecompileMethod Snippet (<ffffffe6>). The purpose of the NOP instructions is to align each snippet
 *       body to the size of a reference so that the JIT entry point is always aligned for atomic patching and to
 *       ensure that any compare and swap (CAS) operations on the JIT entry point are properly aligned to avoid
 *       potential race conditions if both sub-snippets want to patch at the same time.
 *
 *     0x000003ffe064a890 ffffff96 [0x000003ffdb1e95d0]                            Label L0016:
 *     0x000003ffe064a890 ffffff96 [0x000003ffdb1e97c0]                            STG     GPR1, 0(GPR5)
 *     0x000003ffe064a896 ffffff9c [0x000003ffdb1e98b0]                            BASR    GPR4,GPR0
 *     0x000003ffe064a898 ffffff9e [0x000003ffdb1e9a70]                            LG      GPR1, 22(GPR4)
 *     0x000003ffe064a89e ffffffa4 [0x000003ffdb1e9c30]                            LG      GPR4, 14(GPR4)
 *     0x000003ffe064a8a4 ffffffaa [0x000003ffdb1e9d20]                            BCR     BCR(mask=0xf), GPR4
 *     0x000003ffe064a8a6 ffffffac [0x000003ffdb1e9ec0]                            DC      0x000003ff
 *     0x000003ffe064a8aa ffffffb0 [0x000003ffdb1e9fb0]                            DC      0xf1e97368
 *     0x000003ffe064a8ae ffffffb4 [0x000003ffdb1ea0c0]                            DC      0x00000000
 *     0x000003ffe064a8b2 ffffffb8 [0x000003ffdb1ea1b0]                            DC      0x0007d790
 *     0x000003ffe064a8b6 ffffffbc [0x000003ffdb1ea2a0]                            NOP
 *     0x000003ffe064a8b8 ffffffbe [0x000003ffdb1ea410]                            Label L0017:
 *     0x000003ffe064a8b8 ffffffbe [0x000003ffdb1ea510]                            AGHI    GPR5,0xfff0
 *     0x000003ffe064a8bc ffffffc2 [0x000003ffdb1ea610]                            LGR     GPR0,GPR14
 *     0x000003ffe064a8c0 ffffffc6 [0x000003ffdb1ea7d0]                            STG     GPR4, 0(GPR5)
 *     0x000003ffe064a8c6 ffffffcc [0x000003ffdb1ea8c0]                            BASR    GPR4,GPR0
 *     0x000003ffe064a8c8 ffffffce [0x000003ffdb1ea9b0]                            LGR     GPR14,GPR4
 *     0x000003ffe064a8cc ffffffd2 [0x000003ffdb1eab70]                            LG      GPR4, 16(GPR4)
 *     0x000003ffe064a8d2 ffffffd8 [0x000003ffdb1eac60]                            AGHI    GPR14,0x26
 *     0x000003ffe064a8d6 ffffffdc [0x000003ffdb1ead60]                            BCR     BCR(mask=0xf), GPR4
 *     0x000003ffe064a8d8 ffffffde [0x000003ffdb1eaf00]           +--------------> DC      0x000003ff
 *     0x000003ffe064a8dc ffffffe2 [0x000003ffdb1eaff0]           |                DC      0xf15f4f50
 *     0x000003ffe064a8e0 ffffffe6 [0x000003ffdb1eb5d0]           |                NOP
 *     0x000003ffe064a8e2 ffffffe8 [0x000003ffdb1eb0e0]           |   +----------> BRC     J(0xf), Label L0017, labelTargetAddr=0x000003FFE064A8B8
 *     0x000003ffe064a8e6 ffffffec [0x000003ffdb1eb1e0]           |   |   +------> BRC     J(0xf), Label L0016, labelTargetAddr=0x000003FFE064A890
 *     0x000003ffe064a8ea fffffff0 [0x000003ffdb1eb2e0]           |   |   |   +--> DC      0xdeafbeef
 *     0x000003ffe064a8ee fffffff4 [0x000003ffdb1eb3f0]           |   |   |   |    DC      0x000003ff
 *     0x000003ffe064a8f2 fffffff8 [0x000003ffdb1eb4e0]          [4] [3] [2] [1]   DC      0xdc047540
 *     0x000003ffe064a8f6 fffffffc [0x000003ffdb1e33e0]           |   |   |   |    DC      0x00060010
 *     0x000003ffe064a8fa 00000000 [0x000003ffdb1e9480]           +---+---+---+--- LG      GPR1, 0(GPR5)
 *     0x000003ffe064a900 00000006 [0x000003ffdb1e34d0]                            PROC
 *     0x000003ffe064a900 00000006 [0x000003ffdb1eb7b0]                            STG     GPR14, -8(GPR5)
 *     0x000003ffe064a906 0000000c [0x000003ffdb1eb970]                            LAY     GPR5, -40(GPR5)
 *     0x000003ffe064a90c 00000012 [0x000003ffdb1ebb30]                            CLG     GPR5, 80(GPR13)
 *
 *     [1] OFFSET_INTEP_JITEP_SAVE_RESTORE_LOCATION
 *     [2] OFFSET_INTEP_VM_CALL_HELPER_TRAMPOLINE
 *     [3] OFFSET_INTEP_SAMPLING_RECOMPILE_METHOD_TRAMPOLINE
 *     [4] OFFSET_INTEP_SAMPLING_RECOMPILE_METHOD_ADDRESS
 *
 *     2. Counting recompilation
 *
 *     The following sequence of instructions is an excerpt of a counting recompilation of java/lang/Object.<init>()V
 *     on a 64-bit Java 8 SR5 JVM on Linux on z.
 *
 *     The counting recompilation snippet is composed of two sub-snippets:
 *
 *     - CountingRecompileMethod Snippet
 *
 *       The CountingRecompileMethod Snippet (Label L0337) is used to induce a counting recompilation of this method
 *       body. It is emitted at the JIT entry point and executed at runtime on every invocation of the method. The
 *       snippet checks if TR_PersistentJittedBodyInfo._counter is <= 0 and if so it calls a JIT helper (<00000046>)
 *       which induces a recompilation of our method asynchronously by placing it on the method compilation queue. The
 *       compilation thread (re)compiles the method at the new optimization level and calls J9::Recompilation::
 *       methodHasBeenRecompiled. This function then patches the JIT entry point (<00000006>) to point to the
 *       CountingPatchCallSite Snippet (<0000004e>).
 *
 *       responsible
 *
 *     - CountingPatchCallSite Snippet
 *
 *       The CountingPatchCallSite Snippet (Label L0339) is used for patching (at runtime) the caller of our old method
 *       body with a call to the new recompiled method body. The old method code cache can then be reclaimed but the
 *       prologue must be kept intact because there could still be outstanding callers whose call instructions have not
 *       been patched to point to the now recompiled method body.
 *
 *     * Listed below is an illustration of several defined constants used for pre-prologue patching.
 *
 *     * We only need to preserve the CountingPatchCallSite Snippet when freeing up code caches after a recompilation.
 *       Thus we can free the entire code cache except from the JIT entry point to the end of this snippet.
 *
 *     * Methods that cannot be recompiled will have their JIT entry point patched to skip the entire prologue.
 *
 *     0x000003ffe071872a fffffff0 [0x000003ffdb4cf3e0]                            DC      0xdeafbeef
 *     0x000003ffe071872e fffffff4 [0x000003ffdb4cf4f0]                            DC      0x000003ff
 *     0x000003ffe0718732 fffffff8 [0x000003ffdb4cf5e0]                            DC      0xf1e97368
 *     0x000003ffe0718736 fffffffc [0x000003ffdb343dd0]                            DC      0x00060020
 *     0x000003ffe071873a 00000000 [0x000003ffdb4ce3b0]                            LG      GPR1, 0(GPR5)
 *     0x000003ffe0718740 00000006 [0x000003ffdb343ec0]               +---+---+--- PROC
 *     0x000003ffe0718740 00000006 [0x000003ffdb4cf950]               |   |   |    Label L0337:
 *     0x000003ffe0718740 00000006 [0x000003ffdb4cfa50]               |   |   |    AGHI    GPR5,0xffe0
 *     0x000003ffe0718744 0000000a [0x000003ffdb4cfc20]               |   |   |    STMG    GPR1,GPR2, 8(GPR5)
 *     0x000003ffe071874a 00000010 [0x000003ffdb4cfd20]               |   |   |    BASR    GPR4,GPR0
 *     0x000003ffe071874c 00000012 [0x000003ffdb4cfe10]               |   |   |    LGR     GPR1,GPR4
 *     0x000003ffe0718750 00000016 [0x000003ffdb4cff00]               |   |   |    AGHI    GPR4,0xffee
 *     0x000003ffe0718754 0000001a [0x000003ffdb4d00d0]               |   |   |    LG      GPR1, -30(GPR1)
 *     0x000003ffe071875a 00000020 [0x000003ffdb4d0290]               |   |   |    L       GPR2, 0(GPR1)
 *     0x000003ffe071875e 00000024 [0x000003ffdb4d0380]               |   |   |    LTR     GPR2,GPR2
 *     0x000003ffe0718760 00000026 [0x000003ffdb4d04d0]               |   |  [1]   BRC     BNLRC(0xa), Label L0338, labelTargetAddr=0x000003FFE07187BC
 *     0x000003ffe0718764 0000002a [0x000003ffdb4d05d0]               |   |   |    BASR    GPR2,GPR0
 *     0x000003ffe0718766 0000002c [0x000003ffdb4d06c0]               |   |   |    AGHI    GPR2,0x60
 *     0x000003ffe071876a 00000030 [0x000003ffdb4d0890]               |   |   |    STG     GPR2, 0(GPR5)
 *     0x000003ffe0718770 00000036 [0x000003ffdb4d0990]               |   |   |    AGHI    GPR2,0xffba
 *     0x000003ffe0718774 0000003a [0x000003ffdb4d0b60]               |   |   |    LG      GPR2, 0(GPR2)
 *     0x000003ffe071877a 00000040 [0x000003ffdb4d0c50]               |  [2]  |    LGR     GPR0,GPR14
 *     0x000003ffe071877e 00000044 [0x000003ffdb4d0d40]               |   |   |    BCR     BCR(mask=0xf), GPR2
 *     0x000003ffe0718780 00000046 [0x000003ffdb4d0ee0]              [3]  |   |    DC      0x000003ff
 *     0x000003ffe0718784 0000004a [0x000003ffdb4d0fd0]               |   |   |    DC      0xf15f5038
 *     0x000003ffe0718788 0000004e [0x000003ffdb4d1120]               |   |   +--> Label L0339:
 *     0x000003ffe0718788 0000004e [0x000003ffdb4d1220]               |   |        AGHI    GPR5,0xfff0
 *     0x000003ffe071878c 00000052 [0x000003ffdb4d13f0]               |   |        STG     GPR4, 0(GPR5)
 *     0x000003ffe0718792 00000058 [0x000003ffdb4d14f0]               |   |        LGR     GPR0,GPR14
 *     0x000003ffe0718796 0000005c [0x000003ffdb4d15e0]               |   |        BASR    GPR14,GPR0
 *     0x000003ffe0718798 0000005e [0x000003ffdb4d17a0]               |   |        LG      GPR4, 28(GPR14)
 *     0x000003ffe071879e 00000064 [0x000003ffdb4d1890]               |   |        AGHI    GPR14,0xffa2
 *     0x000003ffe07187a2 00000068 [0x000003ffdb4d1a60]               |   |        STG     GPR14, 8(GPR5)
 *     0x000003ffe07187a8 0000006e [0x000003ffdb4d1b50]               |   |        AGHI    GPR14,0xfff4
 *     0x000003ffe07187ac 00000072 [0x000003ffdb4d1d20]               |   |        LG      GPR14, 0(GPR14)
 *     0x000003ffe07187b2 00000078 [0x000003ffdb4d1e10]               |   |        BCR     BCR(mask=0xf), GPR4
 *     0x000003ffe07187b4 0000007a [0x000003ffdb4d1fb0]               |   |        DC      0x000003ff
 *     0x000003ffe07187b8 0000007e [0x000003ffdb4d20a0]               |   |        DC      0xf15f5120
 *     0x000003ffe07187bc 00000082 [0x000003ffdb4d2190]               |   +------> Label L0338:
 *     0x000003ffe07187bc 00000082 [0x000003ffdb4d2320]               |            LMG     GPR1,GPR2, 8(GPR5)
 *     0x000003ffe07187c2 00000088 [0x000003ffdb4d2420]               |            AGHI    GPR5,0x20
 *     0x000003ffe07187c6 0000008c [0x000003ffdb4d2580]               +----------> Label L0340:
 *     0x000003ffe07187c6 0000008c [0x000003ffdb4d2750]                            STG     GPR14, -8(GPR5)
 *     0x000003ffe07187cc 00000092 [0x000003ffdb4d2910]                            LAY     GPR5, -192(GPR5)
 *     0x000003ffe07187d2 00000098 [0x000003ffdb4d2ad0]                            CLG     GPR5, 80(GPR13)
 *
 *     [1] OFFSET_JITEP_COUNTING_PATCH_CALL_SITE_SNIPPET_START
 *     [2] OFFSET_JITEP_COUNTING_PATCH_CALL_SITE_SNIPPET_END
 *     [3] COUNTING_PROLOGUE_SIZE
 */
class TR_S390Recompilation : public TR::Recompilation
   {
   public:
   TR_S390Recompilation(TR::Compilation *);
   static TR::Recompilation         * allocate(TR::Compilation *);

   /** \brief
    *     Generates the pre-prologue which may consist of the VM Call Helper Snippet and the SamplingRecompileMethod
    *     Snippet.
    *
    *  \return
    *     The last generated instruction.
    */
   virtual TR::Instruction* generatePrePrologue();

   /** \brief
    *     Generates the prologue which consists of the CountingRecompileMethod Snippet and the SamplingRecompileMethod
    *     Snippet.
    *
    *  \param cursor
    *     The cursor to which the generated instructions will be appended.
    *
    *  \return
    *     The last generated instruction.
    */
   virtual TR::Instruction* generatePrologue(TR::Instruction* cursor);

   virtual void                      postCompilation();
   virtual TR_PersistentMethodInfo * getExistingMethodInfo(TR_ResolvedMethod *method);

   void setLoadArgSize(int32_t size)
      {
      _loadArgumentSize = size;
      }

   private:

   int32_t _loadArgumentSize; // size of load arguments instr

   /** \brief
    *     The data constant instruction pointing to the body info address.
    *
    *  \note
    *     The data constant is encoded in the pre-prologue and is used in both the sampling and counting
    *     recompilation sequences emitted in the pre-prologue and prologue respectively. This pointer is used to
    *     calculate the offset within a memory reference to load the body info at runtime.
    */
   TR::Instruction* bodyInfoDataConstant;
   };

#define FOUR_BYTE_JUMP_INSTRUCTION 0xA7F40000

#endif
