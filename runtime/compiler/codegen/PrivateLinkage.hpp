/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef J9_PRIVATELINKAGE_INCL
#define J9_PRIVATELINKAGE_INCL

#include "codegen/Linkage.hpp"
#include "env/jittypes.h"
#include "infra/Assert.hpp"

namespace TR { class CodeGenerator; }

namespace J9
{

class PrivateLinkage : public TR::Linkage
   {
public:

   PrivateLinkage(TR::CodeGenerator *cg) :
         TR::Linkage(cg)
      {
      }

   /**
    * @class LinkageInfo
    *
    * @brief Helper class for encoding and decoding the linkage info word that is
    *    embedded in the code cache prior to the interpreter entry point in each
    *    compiled method body.
    *
    * @details
    *     Implementation restrictions:
    *     * this is a non-instantiable abstract class
    *     * this class cannot have any virtual methods
    */
   class LinkageInfo
      {
   public:
      static LinkageInfo *get(void *startPC) { return reinterpret_cast<LinkageInfo *>(((uint32_t*)startPC)-1); }

      void setCountingMethodBody() { _word |= CountingPrologue; }
      void setSamplingMethodBody() { _word |= SamplingPrologue; }
      void setHasBeenRecompiled()
         {
         TR_ASSERT((_word & HasFailedRecompilation)==0, "Cannot setHasBeenRecompiled because method has failed recompilation");
         _word |= HasBeenRecompiled;
         }

      void setHasFailedRecompilation()
         {
         TR_ASSERT((_word & HasBeenRecompiled) == 0, "Cannot setHasFailedRecompilation because method has been recompiled");
         _word |= HasFailedRecompilation;
         }

      void setIsBeingRecompiled() { _word |= IsBeingRecompiled; }
      void resetIsBeingRecompiled() { _word &= ~IsBeingRecompiled; }

      bool isCountingMethodBody() { return (_word & CountingPrologue) != 0; }
      bool isSamplingMethodBody() { return (_word & SamplingPrologue) != 0; }
      bool isRecompMethodBody() { return (_word & (SamplingPrologue | CountingPrologue)) != 0; }
      bool hasBeenRecompiled() { return (_word & HasBeenRecompiled) != 0; }
      bool hasFailedRecompilation() { return (_word & HasFailedRecompilation) != 0; }
      bool recompilationAttempted() { return hasBeenRecompiled() || hasFailedRecompilation(); }
      bool isBeingCompiled() { return (_word & IsBeingRecompiled) != 0; }

      inline uint16_t getReservedWord() { return (_word & ReservedMask) >> 16; }
      inline void setReservedWord(uint16_t w) { _word |= ((w << 16) & ReservedMask); }

      int32_t getJitEntryOffset()
         {
#if defined(TR_TARGET_X86) && defined(TR_TARGET_32BIT)
         return 0;
#else
         return getReservedWord();
#endif
         }

      enum
         {
         ReturnInfoMask                     = 0x0000000F, // bottom 4 bits
         // The VM depends on these four bits - word to the wise: don't mess

         SamplingPrologue                   = 0x00000010,
         CountingPrologue                   = 0x00000020,
         // NOTE: flags have to be set under the compilation monitor or during compilation process
         HasBeenRecompiled                  = 0x00000040,
         HasFailedRecompilation             = 0x00000100,
         IsBeingRecompiled                  = 0x00000200,

         // RESERVED:
         // non-ia32:                         0xffff0000 <---- jitEntryOffset
         // ia32:                             0xffff0000 <---- Recomp/FSD save area

         ReservedMask                       = 0xFFFF0000
         };

      uint32_t _word;

   private:
      LinkageInfo() {};
      };

   /**
    * @brief J9 private linkage override of OMR function
    */
   virtual intptr_t entryPointFromCompiledMethod();

   /**
    * @brief J9 private linkage override of OMR function
    */
   virtual intptr_t entryPointFromInterpretedMethod();

   virtual void mapIncomingParms(TR::ResolvedMethodSymbol *method);
   };

}

#endif
