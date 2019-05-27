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

#ifndef X86RECOMPILATION_INCL
#define X86RECOMPILATION_INCL

#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
namespace TR { class CodeGenerator; }
class TR_ResolvedMethod;

// ***************************************************************************
//
// Recompilation Support Runtime methods
//
// Methods headers look different based on the type of compilation: there are
// inherent differences between sampling and counting compilations and yet
// again between counting with profiling and counting without preexistence headers.
//
// The linkage info field contains information about what kind of compilation
// produced this method.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// When changing the prologue or preprologue shape/size, all recompilation-related
// code must be revisited to make sure it is kept consistent.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

//
// Sampling Compilation
// ====================
//
// If preexistence was not performed on this method, the following is what the
// prologue looks like:
//
//              -16 db[3]   padding 3 bytes
//              -13 call    _samplingRecompileMethod <-- immediate field aligned on 4-byte bndry
//              -8  dd      address of persistent jitted body info
//              -4  dd      method linkage info and flags
//               0  ??      first instruction of the method (must be atleast 2 bytes)
//
// With preexistence the prologue looks like the following
//
//              -24 db      padding 1 byte
//              -23 mov     edi, j9method
//              -18 call    interpretedCallGlue
//              -13 call    _samplingRecompileMethod <-- immediate field aligned on 4 byte bndry
//              -8  dd      address of persistent jitted body info
//              -4  dd      method linkage info and flags
//               0  ??      first instruction of the method (must be atleast 2 bytes)
//
// The first instruction of the method can be either of the following types
//    push ebx              (deprecated) used when using ebx as dedicated bp
//                            (this instruction is forced to be 2 bytes)
//                            (there is a debug flag in ia32Code that enabled this)
//    sub  esp, byte        default when the stack frame is small
//    sub  esp, dword       default when the stack frame is large
//
// To recompile, the first instruction of the method is overwritten to be a
// jump to the call instruction at (startPC-13).
//
// It is not possible to have a sampling body without recompilation.
//
// Counting Recompilation
// ======================
// The method header looks like the following:
// Without Preexistence
//              -8  dd      address of persistent jitted body info
//              -4  dd      method linkage info flags
//     startPC + 0  ??      first instruction of the method
//
// With Preexistence
//              -20 dw      padding 2 bytes
//              -18 mov     edi, j9method
//              -13 call    interpreterCallGlue
//              -8  dd      address of persistent jitted body info
//              -4  dd      method linkage info flags
//     startPC + 0  ??      first instruction of the method
//
// The prologue of the method looks like the following:
// When Profiling
//                  cmp  [recompilationCounter], 0
//                  jl   LrecompSnippet
// Otherwise
//                  sub  [recompilationCounter], 1
//                  jl   LrecompSnippet
//
// recompilationCounter is the first field of persistent jitted body info, so the
// address contained in this instruction and at startPC - 8 is the same.
// (Duplication to minimize the thread safety issues that come up when
//  patching the first instruction)
//
// ***************************************************************************

// We define offsets relative to startPC (ie. the interpreter entry point)
// because that way they are constant.
//
#if defined(TR_TARGET_64BIT)

#  define START_PC_TO_ITR_GLUE_SAMPLING             (-21)
#  define START_PC_TO_ORIGINAL_ENTRY_BYTES          (-19)
#  define START_PC_TO_RECOMPILE_SAMPLING            (-17)
#  define START_PC_TO_ITR_GLUE_COUNTING             (-16)
#  define START_PC_TO_METHOD_INFO_ADDRESS           (-12)

#  define COUNTING_PROLOGUE_SIZE                    (19)

#  define COUNTING_RECOMPILE_METHOD           TR_AMD64countingRecompileMethod
#  define SAMPLING_RECOMPILE_METHOD           TR_AMD64samplingRecompileMethod
#  define COUNTING_PATCH_CALL_SITE            TR_AMD64countingPatchCallSite
#  define SAMPLING_PATCH_CALL_SITE            TR_AMD64samplingPatchCallSite

#else

#  define START_PC_TO_ITR_GLUE_SAMPLING             (-23)
#  define START_PC_TO_ITR_GLUE_COUNTING             (-18)
#  define START_PC_TO_RECOMPILE_SAMPLING            (-13)
#  define START_PC_TO_METHOD_INFO_ADDRESS           (-8)
#  define START_PC_TO_ORIGINAL_ENTRY_BYTES          (-2)

#  define COUNTING_PROLOGUE_SIZE                    (13)

#  define COUNTING_RECOMPILE_METHOD           TR_IA32countingRecompileMethod
#  define SAMPLING_RECOMPILE_METHOD           TR_IA32samplingRecompileMethod
#  define COUNTING_PATCH_CALL_SITE            TR_IA32countingPatchCallSite
#  define SAMPLING_PATCH_CALL_SITE            TR_IA32samplingPatchCallSite

#endif

#define CALL_INSTRUCTION                   0xE8
#define TWO_BYTE_JUMP_INSTRUCTION          0xEB
#define SPIN_LOOP_INSTRUCTION              0xFEEB


class TR_X86Recompilation : public TR::Recompilation
   {
   public:

   TR_X86Recompilation(TR::Compilation *);

   static TR::Recompilation * allocate(TR::Compilation *);

   virtual TR_PersistentMethodInfo *getExistingMethodInfo(TR_ResolvedMethod *method);
   virtual TR::Instruction *generatePrePrologue();
   virtual TR::Instruction *generatePrologue(TR::Instruction *);
   virtual void postCompilation();

   TR::CodeGenerator *cg() { return _compilation->cg(); }

   private:
   void setMethodReturnInfoBits();
   };

#endif
