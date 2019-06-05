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

#include "j9.h"
#include "j9consts.h"
#include "codegen/FrontEnd.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeRuntime.hpp"
#include "runtime/J9Runtime.hpp"
#include "control/CompilationRuntime.hpp"

#define PPC_INSTRUCTION_LENGTH 4

#if defined(TR_TARGET_POWER)

#if defined(TR_TARGET_64BIT)
#define TRAMPOLINE_SIZE       28
#define OFFSET_IPIC_TO_CALL   36
#else
#define TRAMPOLINE_SIZE       16
#define OFFSET_IPIC_TO_CALL   32
#endif

extern TR_Processor portLibCall_getProcessorType();

#ifdef TR_HOST_POWER
extern void     ppcCodeSync(uint8_t *, uint32_t);
#endif

void ppcCodeCacheConfig(int32_t ccSizeInByte, int32_t *numTempTrampolines)
   {
   // Estimated: 2KB per method, with 10% being recompiled(multi-times)

   *numTempTrampolines = ccSizeInByte>>12;
   }

void ppcCreateHelperTrampolines(uint8_t *trampPtr, int32_t numHelpers)
   {
   TR::CodeCacheConfig &config = TR::CodeCacheManager::instance()->codeCacheConfig();
   static bool customP4 =  feGetEnv("TR_CustomP4Trampoline") ? true : false;
   static TR_Processor proc = customP4 ? portLibCall_getProcessorType() :
      TR_DefaultPPCProcessor;

   uint8_t *bufferStart = trampPtr, *buffer;
   for (int32_t cookie=1; cookie<numHelpers; cookie++)
      {
      intptrj_t helper = (intptrj_t)runtimeHelperValue((TR_RuntimeHelper)cookie);
      // Skip over the first one for index 0
      bufferStart += config.trampolineCodeSize();
      buffer = bufferStart;

#if defined(TR_TARGET_64BIT)
         // ld gr11, [grPTOC, 8*(cookie-1)]
         *(int32_t *)buffer = 0xe9700000 | (((cookie-1)*sizeof(intptrj_t)) & 0x0000ffff);
         buffer += 4;

#else
         // For POWER4 which has a problem with the CTR/LR cache when the upper
         // bits are not 0 extended.. Use li/oris when the 16th bit is off
         if( !(helper & 0x00008000) )
            {
            // li r11, lower
            *(int32_t *)buffer = 0x39600000 | (helper & 0x0000ffff);
            buffer += 4;
            // oris r11, r11, upper
            *(int32_t *)buffer = 0x656b0000 | ((helper>>16) & 0x0000ffff);
            buffer += 4;
            }
         else
            {
            // lis r11, upper
            *(int32_t *)buffer = 0x3d600000 | (((helper>>16) + (helper&(1<<15)?1:0)) & 0x0000ffff);
            buffer += 4;

            // addi r11, r11, lower
            *(int32_t *)buffer = 0x396b0000 | (helper & 0x0000ffff);
            buffer += 4;

            // Now, if highest bit is on we need to clear the sign extend bits on 64bit CPUs
            // ** POWER4 pref fix **
            if( (helper & 0x80000000) && (!customP4 || proc == TR_PPCgp))
               {
               // rlwinm r11,r11,sh=0,mb=0,me=31
               *(int32_t *)buffer = 0x556b003e;
               buffer += 4;
               }
            }

#endif

      // mtctr r11
      *(int32_t *)buffer = 0x7d6903a6;
      buffer += 4;

      // bctr
      *(int32_t *)buffer = 0x4e800420;
      buffer += 4;
   }
#ifdef TR_HOST_POWER
   ppcCodeSync(trampPtr, config.trampolineCodeSize() * numHelpers);
#endif

   }

void ppcCreateMethodTrampoline(void *trampPtr, void *startPC, void *method)
   {
   static bool customP4 =  feGetEnv("TR_CustomP4Trampoline") ? true : false;
   static TR_Processor proc = customP4 ? portLibCall_getProcessorType() :
      TR_DefaultPPCProcessor;
   uint8_t *buffer = (uint8_t *)trampPtr;
   TR_LinkageInfo *linkInfo = TR_LinkageInfo::get(startPC);
   intptrj_t dispatcher = (intptrj_t)((uint8_t *)startPC + linkInfo->getReservedWord());

      // Take advantage of both gr0 and gr11 ...
#if defined(TR_TARGET_64BIT)
      // lis gr0, upper 16-bits
      *(int32_t *)buffer = 0x3c000000 | ((dispatcher>>48) & 0x0000ffff);
      buffer += 4;

      // lis gr11, bits 32--47
      *(int32_t *)buffer = 0x3d600000 | ((dispatcher>>16) & 0x0000ffff);
      buffer += 4;

      // ori gr0, gr0, bits 16-31
      *(int32_t *)buffer = 0x60000000 | ((dispatcher>>32) & 0x0000ffff);
      buffer += 4;

      // ori gr11, gr11, bits 48--63
      *(int32_t *)buffer = 0x616b0000 | (dispatcher & 0x0000ffff);
      buffer += 4;

      // rldimi gr11, gr0, 32, 0
      *(int32_t *)buffer = 0x780b000e;
      buffer += 4;
#else
      // For POWER4 which has a problem with the CTR/LR cache when the upper
      // bits are not 0 extended. Use li/oris when the 16th bit is off
      if (customP4)
         {
         if ( !(dispatcher & 0x00008000) )
            {
            // li r11, lower
            *(int32_t *)buffer = 0x39600000 | (dispatcher & 0x0000ffff);
            buffer += 4;

            // oris r11, r11, upper
            *(int32_t *)buffer = 0x656b0000 | ((dispatcher>>16) & 0x0000ffff);
            buffer += 4;
            }
         else
            {
            // lis gr11, upper
            *(int32_t *)buffer = 0x3d600000 |
               (((dispatcher>>16) + (dispatcher&(1<<15)?1:0)) & 0x0000ffff);
            buffer += 4;

            // addi gr11, gr11, lower
            *(int32_t *)buffer = 0x396b0000 | (dispatcher & 0x0000ffff);
            buffer += 4;

            // Now, if highest bit is on we need to clear the sign extend bits on 64bit CPUs
            // ** POWER4 pref fix **
            if ( (dispatcher & 0x80000000) && proc == TR_PPCgp)
               {
               // rlwinm r11,r11,sh=0,mb=0,me=31
               *(int32_t *)buffer = 0x556b003e;
               buffer += 4;
               }
            }
         }
      else
         {
         // lis gr11, upper
         *(int32_t *)buffer = 0x3d600000 |
            (((dispatcher>>16) + (dispatcher&(1<<15)?1:0)) & 0x0000ffff);
         buffer += 4;

         // addi gr11, gr11, lower
         *(int32_t *)buffer = 0x396b0000 | (dispatcher & 0x0000ffff);
         buffer += 4;
         }
#endif

   // mtctr gr11
   *(int32_t *)buffer = 0x7d6903a6;
   buffer += 4;

   // bcctr
   *(int32_t *)buffer = 0x4e800420;

#if defined(TR_HOST_POWER)
   TR::CodeCacheConfig &config = TR::CodeCacheManager::instance()->codeCacheConfig();
   ppcCodeSync((uint8_t *)trampPtr, config.trampolineCodeSize());
#endif

// TODO: X-compilation support and tprof support
   }


static bool isInterfaceCallSite(uint8_t *callSite, int32_t& distanceToActualCallSite)
   {
   // Looking for a pattern of:
   //[  bne or b Snippet  ]  ; The usual case, typically bne, but b if the snippet cannot be reached with bne
   //    -or-
   //[  bne or b Snippet  ]  ; The case where lastITable is checked inline to find the vft offset and a virtual call is performed
   //[  ...               ]
   //[  ...               ]
   //[  ...               ]
   //[  ...               ]
   //[  b Call            ]
   //   ...
   // Call:
   //   mtctr r11
   //   bctrl                <----- Actual Call Site.
   // After:
   //   ...
   //
   //   ...
   // Snippet:
   //   bl ...               <----- "callSite".
   //   b After

   // Assumed true if this function is being called
#if 0
   // Check for bl
   if (*callSite & 0xfc000001 != 0x48000001)
      return false;
#endif

   // Check for b
   if ((*(int32_t *)(callSite + PPC_INSTRUCTION_LENGTH) & 0xfc000001) != 0x48000000)
      return false;

   distanceToActualCallSite = (*(int32_t*)(callSite + PPC_INSTRUCTION_LENGTH) & 0x03fffffc) << 6 >> 6; //Mask to get [LI,AA,LK], LSH by 6 to sign extend, then RSH by 6 to get 8 byte distance.
   uint8_t *actualCallSite = callSite + distanceToActualCallSite;

   // Check for bctrl
   if (*(int32_t*)actualCallSite != 0x4e800421)
      return false;

   // Check for mtctr r11
   if (*(int32_t*)(actualCallSite - PPC_INSTRUCTION_LENGTH) != 0x7d6903a6)
      return false;

   // Check for bne or b jumping to the interface call snippet or b to mtctr
   uint8_t *branchToIPIC = actualCallSite - (PPC_INSTRUCTION_LENGTH*3);
   int32_t  distanceToIPIC;

#ifdef INLINE_LASTITABLE_CHECK
   // First check if this is a b
   if ((*(int32_t*)branchToIPIC & 0xfc000000) == 0x48000000)
      {
      distanceToIPIC = (*(int32_t*)branchToIPIC & 0x03FFFFFC) << 6 >> 6; //Mask to get [LI,AA,LK], LSH by 6 to sign extend, then RSH by 6 to get 8 byte distance.
      // And then check if it branches to a mtctr r11
      if (distanceToIPIC == (PPC_INSTRUCTION_LENGTH*2) &&
          *(int32_t*)(branchToIPIC + distanceToIPIC) == 0x7d6903a6)
         {
         // If so, this looks like a lastITable call, look back earlier for the branch to the snippet
         branchToIPIC -= 4*PPC_INSTRUCTION_LENGTH;
         }
      }
#endif /* INLINE_LASTITABLE_CHECK */

   // Now check for bne or b jumping to interface call snippet
   if ((*(int32_t*)branchToIPIC & 0xffff0000) == 0x40820000)
      {
      //Check bne jumps to IPIC
      distanceToIPIC = (*(int32_t*)branchToIPIC & 0x0000FFFC) << 16 >> 16; //Mask to get [BD,AA,LK], LSH by 16 to sign extend, then RSH by 16 to get 8 byte distance.
      }
   else if ((*(int32_t*)branchToIPIC & 0xfc000000) == 0x48000000)
      {
      //Check b jumps to IPIC
      distanceToIPIC = (*(int32_t*)branchToIPIC & 0x03FFFFFC) << 6 >> 6; //Mask to get [LI,AA,LK], LSH by 6 to sign extend, then RSH by 6 to get 8 byte distance.
      }
   else
      return false;

   if (distanceToIPIC - (PPC_INSTRUCTION_LENGTH*3) + distanceToActualCallSite != 0)
      return false;

   return true;

   }

bool ppcCodePatching(void *method, void *callSite, void *currentPC, void *currentTramp, void *newPC, void *extra)
   {
   TR_LinkageInfo *linkInfo = TR_LinkageInfo::get(newPC);
   uint8_t        *entryAddress = (uint8_t *)newPC + linkInfo->getReservedWord();
   intptrj_t       distance;
   uint8_t        *patchAddr;
   int32_t         currentDistance, oldBits = *(int32_t *)callSite;
   bool            isLinkStackPreservingIPIC = false;
   int32_t         distanceToActualCallSite;

   if ((oldBits & 0xFC000001) == 0x48000001 && !(isLinkStackPreservingIPIC = isInterfaceCallSite((uint8_t *)callSite, distanceToActualCallSite)))
      {
      patchAddr = (uint8_t *)callSite;
      distance = entryAddress - (uint8_t *)callSite;
      currentDistance = ((oldBits << 6) >> 6) & 0xfffffffc;
      oldBits &= 0xfc000003;
      if (TR::Options::getCmdLineOptions()->getOption(TR_StressTrampolines) ||
          !TR::Compiler->target.cpu.isTargetWithinIFormBranchRange((intptrj_t)entryAddress, (intptrj_t)callSite))
         {
         if (currentPC == newPC)
            {
            distance = (uint8_t *)currentTramp - patchAddr;
            }
         else
            {
            void *newTramp = mcc_replaceTrampoline(reinterpret_cast<TR_OpaqueMethodBlock *>(method), callSite, currentTramp, currentPC, newPC, true);
            if (newTramp == NULL)
               {
               //if (currentTramp == NULL)
                  //FIXME we need an assume for runtime as well - TR_ASSERT(0, "This is an internal error.\n");
               return false;
               }
            ppcCreateMethodTrampoline(newTramp, newPC, method);
            if (currentTramp == NULL)
               {
               distance = (uint8_t *)newTramp - patchAddr;
               }
            else
               {
               if (currentDistance != ((uint8_t *)currentTramp - patchAddr))
                  {
                  oldBits |= ((uint8_t *)currentTramp - patchAddr) & 0x03fffffc;
                  *(int32_t *)patchAddr = oldBits;
#if defined(TR_HOST_POWER)
                  ppcCodeSync(patchAddr, 4);
#endif
                  }

               patchAddr = (uint8_t *)currentTramp;
               distance = (uint8_t *)newTramp - patchAddr;
               currentDistance = 0;
               oldBits = 0x48000000;
               }
            }
         }

      if (currentDistance == distance)
         return true;
      oldBits |= distance & 0x03fffffc;
      *(int32_t *)patchAddr = oldBits;

#if defined(TR_HOST_POWER)
      ppcCodeSync(patchAddr, 4);
#endif
      }
   else if (oldBits==0x4e800421 || isLinkStackPreservingIPIC)      // It is a bctrl or an IPIC bl
      {
      if (isLinkStackPreservingIPIC)
         {
         // If this is a link stack preserving IPIC then callSite points to the bl in the snippet, not
         // the bctrl in jitted code; adjust it here so that it points to jitted code

         callSite = (uint8_t *)callSite + distanceToActualCallSite;
         }

      oldBits = *(int32_t *)((uint8_t *)callSite - 4);
      if (oldBits == 0x7d6903a6)      // mtctr r11 used in interface dispatch
         {
         // This is the distance from the call site back to the first instruction of the IPIC sequence
         int32_t encodingStartOffset = OFFSET_IPIC_TO_CALL;

         oldBits = *(int32_t *)((uint8_t *)callSite - 12);
         if ((oldBits & 0xFC000000) == 0x48000000)      // Check if this is a b (instead of a bne)
            {
#ifdef INLINE_LASTITABLE_CHECK
            int32_t branchOffset = (oldBits & 0x03FFFFFC) << 6 >> 6;

            if (branchOffset == PPC_INSTRUCTION_LENGTH*2)
               // This is a b to the mtctr because this is a call using the lastITable, add # of extra instructions generated for it
               encodingStartOffset += 10 * PPC_INSTRUCTION_LENGTH;
            else
#endif /* INLINE_LASTITABLE_CHECK */
               // This is a b to the snippet because it was too far to reach using a bne, add 1 instruction to the distance
               encodingStartOffset += 4;
            }

         oldBits = *(int32_t *)((uint8_t *)callSite - encodingStartOffset);     // The load of the first IPIC cache slot or rldimi
#if defined(TR_TARGET_64BIT)
         if (((oldBits>>26) & 0x0000003F) != 30)
            {
            // PTOC was used, oldBits is ld
            currentDistance = oldBits<<16>>16;
            if (((oldBits>>16) & 0x0000001F) == 12)
               {
               oldBits = *(int32_t *)((uint8_t *)callSite - encodingStartOffset - 4);
               currentDistance += oldBits<<16;
               }
            patchAddr = *(uint8_t **)(*(intptrj_t *)extra + currentDistance);
            }
         else
            {
            // PTOC was full and the load address is formed via 5 instructions: lis, lis, ori, rldimi, ldu; oldBits is the rldimi
            distance  = ((intptrj_t)(*(int32_t *)((uint8_t *)callSite - encodingStartOffset - 12)) & 0x0000FFFF) << 48;
            distance |= ((intptrj_t)(*(int32_t *)((uint8_t *)callSite - encodingStartOffset - 8)) & 0x0000FFFF) << 16;
            distance |= ((intptrj_t)(*(int32_t *)((uint8_t *)callSite - encodingStartOffset - 4)) & 0x0000FFFF) << 32;
            distance += ((intptrj_t)(*(int32_t *)((uint8_t *)callSite - encodingStartOffset + 4)) & 0x0000FFFC) << 48 >> 48;
            patchAddr = (uint8_t *)distance;
            }
#else
         // oldBits is lwzu
         currentDistance = *(int32_t *)((uint8_t *)callSite - encodingStartOffset - 4);
         patchAddr = (uint8_t *)((currentDistance<<16) + (oldBits<<16>>16));
#endif
         // patchAddr now points to the class ptr of the first cache slot

         const intptrj_t *obj = *(intptrj_t **)((intptrj_t)extra + sizeof(intptrj_t));
         // Discard high order 32 bits via cast to uint32_t to avoid shifting and masking when using compressed refs
#if defined(OMR_GC_COMPRESSED_POINTERS)
         intptrj_t currentReceiverJ9Class = *(uint32_t *)((int8_t *)obj + TMP_OFFSETOF_J9OBJECT_CLAZZ);
#else
         intptrj_t currentReceiverJ9Class = *(intptrj_t *)((int8_t *)obj + TMP_OFFSETOF_J9OBJECT_CLAZZ);
#endif

         // Throwing away the flag bits in CLASS slot
         currentReceiverJ9Class &= ~(J9_REQUIRED_CLASS_ALIGNMENT - 1);

         if (*(intptrj_t *)patchAddr == currentReceiverJ9Class)
            {
            *(intptrj_t *)(patchAddr+sizeof(intptrj_t)) = (intptrj_t)entryAddress;
            }
         else if (*(intptrj_t *)(patchAddr+2*sizeof(intptrj_t)) == currentReceiverJ9Class)
            {
            *(intptrj_t *)(patchAddr+3*sizeof(intptrj_t)) = (intptrj_t)entryAddress;
            }
         }
      else if (oldBits != 0x7d8903a6) // mtctr r12 used in virtual dispatch
         {
         // Don't recognize the calling site: error condition
         //TR_ASSERT(0, "Got a unrecognizable calling site.\n");
         }
      }
   else
      {
      // Don't recognize the calling site: error condition
      //TR_ASSERT(0, "Got a unrecognizable calling site.\n");
      }

   return true;
   }

void ppcCodeCacheParameters(int32_t *trampolineSize, void **callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   static bool customP4 =  feGetEnv("TR_CustomP4Trampoline") ? true : false;

#if defined(TR_TARGET_64BIT)
   *trampolineSize = TRAMPOLINE_SIZE;
#else
   if (customP4)
      {
      *trampolineSize = portLibCall_getProcessorType() == TR_PPCgp ? TRAMPOLINE_SIZE + 4 : TRAMPOLINE_SIZE;
      }
   else
      {
      *trampolineSize = TRAMPOLINE_SIZE + 4;
      }
#endif
   //TR::CodeCacheConfig &config = TR::CodeCacheManager::instance()->codeCacheConfig();
   //fprintf(stderr, "Processor Offset: %d\n", portLibCall_getProcessorType() - TR_FirstPPCProcessor);
   //fprintf(stderr, "Trampoline Size: %d, %d\n", *trampolineSize, config.trampolineCodeSize);
   callBacks[0] = (void *)&ppcCodeCacheConfig;
   callBacks[1] = (void *)&ppcCreateHelperTrampolines;
   callBacks[2] = (void *)&ppcCreateMethodTrampoline;
   callBacks[3] = (void *)&ppcCodePatching;
   callBacks[4] = (void *)TR::createCCPreLoadedCode;
   *CCPreLoadedCodeSize = TR::getCCPreLoadedCodeSize();
   *numHelpers = TR_PPCnumRuntimeHelpers;
   }

#undef TRAMPOLINE_SIZE

#endif /*(TR_TARGET_POWER)*/

#if defined(TR_TARGET_X86) && defined(TR_TARGET_64BIT)
#include "x/runtime/X86Runtime.hpp"

// Hack markers
//
// TODO:AMD64: We should obtain the actual boundary, but 8 is conservatively safe
//
#define INSTRUCTION_PATCH_ALIGNMENT_BOUNDARY (8)

#if defined(TR_TARGET_64BIT)
/*FXME this IS_32BIT_RIP is already define in different places; should be moved to a header file*/
#define IS_32BIT_RIP(x,rip)  ((intptrj_t)(x) == (intptrj_t)(rip) + (int32_t)((intptrj_t)(x) - (intptrj_t)(rip)))
#define TRAMPOLINE_SIZE    16
#define CALL_INSTR_LENGTH  5

void amd64CodeCacheConfig(int32_t ccSizeInByte, uint32_t *numTempTrampolines)
   {
   // AMD64 method trampoline can be modified in place.
   *numTempTrampolines = 0;
   }

void amd64CreateHelperTrampolines(uint8_t *trampPtr, int32_t numHelpers)
   {
   uint8_t *bufferStart = trampPtr, *buffer;

   for (int32_t i=1; i<numHelpers; i++)
      {
      intptrj_t helperAddr = (intptrj_t)runtimeHelperValue((TR_RuntimeHelper)i);

      // Skip the first trampoline for index 0
      bufferStart += TRAMPOLINE_SIZE;
      buffer = bufferStart;

      // JMP [RIP]
      // DQ  helperAddr
      // 2-byte padding
      //
      *(uint16_t *)buffer = 0x25ff;
      buffer += 2;
      *(uint32_t *)buffer = 0x00000000;
      buffer += 4;
      *(int64_t *)buffer = helperAddr;
      buffer += 8;
      *(uint16_t *)buffer = 0x9090;
      }
   }

void amd64CreateMethodTrampoline(void *trampPtr, void *startPC, TR_OpaqueMethodBlock *method)
   {
   J9Method *ramMethod = reinterpret_cast<J9Method *>(method);
   uint8_t *buffer = (uint8_t *)trampPtr;
   TR_LinkageInfo *linkInfo = TR_LinkageInfo::get(startPC);
   intptrj_t dispatcher = (intptrj_t)((uint8_t *)startPC + linkInfo->getReservedWord());

   // The code below is disabled because of a problem with direct call to JNI methods
   // through trampoline. The problem is that at the time of generation of the trampoline
   // we don't know if we did directToJNI call or we are calling the native thunk. In case
   // we are calling the thunk the code below won't work and it will give the caller address
   // of the C native and we crash.
   if (0 && ramMethod && (((UDATA)ramMethod->constantPool) & J9_STARTPC_JNI_NATIVE))
      {
      // !!! !!! !!! NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE !!! !!! !!!
      //
      // the following sequence is hardcoded in JitRuntime.cpp in _patchJNICallSite
      // When making changes in the sequence above, make sure to go and update the amd64
      // version of the patchJNICallSite routine.
      //
      if (TR::CompilationInfo::isCompiled(ramMethod))
         startPC = (void *)(*((uintptrj_t *)((uint8_t *)startPC - 12)));

      *buffer++ = 0x49;
      *buffer++ = 0xbb;
      *(intptrj_t *)buffer = (intptrj_t)startPC;
      buffer += sizeof(intptrj_t);

      *buffer++ = 0x41;
      *buffer++ = 0xff;
      *buffer++ = 0xe3;

      *buffer++ = 0x90;
      *buffer++ = 0x90;
      *buffer = 0x90;
      }

   // Use RDI as the scratch register:
   // MOV RDI, dispatcher
   // JMP RDI
   // 3-byte padding
   //

   else
      {
      *buffer++ = 0x48;
      *buffer++ = 0xbf;
      *(intptrj_t *)buffer = dispatcher;
      buffer += sizeof(intptrj_t);

      *buffer++ = 0x48;
      *buffer++ = 0xff;
      *buffer++ = 0xe7;

      *buffer++ = 0x90;
      *buffer++ = 0x90;
      *buffer = 0x90;
      }
   }

int32_t amd64CodePatching(void *theMethod, void *callSite, void *currentPC, void *currentTramp, void *newPC, void *extra)
   {
   J9Method *method = reinterpret_cast<J9Method *>(theMethod);
   // We already checked the call site for immediate call. No need to check again ...
   //
   TR_LinkageInfo *linkInfo = TR_LinkageInfo::get(newPC);
   uint8_t        *entryAddress = (uint8_t *)newPC + linkInfo->getReservedWord();
   uint8_t        *patchAddr = (uint8_t *)callSite;
   intptrj_t       distance;
   int32_t         currentDistance;

   currentDistance = *(int32_t *)(patchAddr+1);
   distance = (intptrj_t)entryAddress - (intptrj_t)patchAddr - CALL_INSTR_LENGTH;
   if (TR::Options::getCmdLineOptions()->getOption(TR_StressTrampolines) || !IS_32BIT_RIP(distance, 0))
      {
      if (currentPC == newPC)
         {
         distance = (uint8_t *)currentTramp - (uint8_t *)patchAddr - CALL_INSTR_LENGTH;
         }
      else
         {
         void *newTramp = mcc_replaceTrampoline(reinterpret_cast<TR_OpaqueMethodBlock *>(method), callSite, currentTramp, currentPC, newPC, false);
         //TR_ASSERT(newTramp!=NULL && (currentTramp==NULL || newTramp==currentTramp), "This is an internal error.\n");
         distance = (uint8_t *)newTramp - (uint8_t *)patchAddr - CALL_INSTR_LENGTH;
         if (currentTramp == NULL)
            amd64CreateMethodTrampoline(newTramp, newPC, reinterpret_cast<TR_OpaqueMethodBlock *>(method));
         else
            {
            // Patch the existing method trampoline.
            //

            // Self-loop
            *(uint16_t *)currentTramp = 0xfeeb;
            patchingFence16(currentTramp);

            // Store the new address
            *(intptrj_t *)((uint8_t *)currentTramp + 2) = (intptrj_t)entryAddress;
            patchingFence16(currentTramp);

            // Restore the MOV instruction
            *(uint16_t *)currentTramp = 0xbf48;
            }
         }
      }

   //TR_ASSERT(IS_32BIT_RIP(distance, 0), "Call target should be reachable directly.\n");
   if (currentDistance != distance)
      {
      // Patch the call displacement
      //
      if (((uintptrj_t)patchAddr+4) % INSTRUCTION_PATCH_ALIGNMENT_BOUNDARY >= 3)
         {
         // Displacement is entirely between the boundaries, so just patch it
         //
         *(uint32_t *)(patchAddr+1) = distance;
         }
      else
         {
         // Must use self-loop
         //
         //TR_ASSERT(((uintptrj_t)patchAddr+1) % INSTRUCTION_PATCH_ALIGNMENT_BOUNDARY != 0,
         //   "Self-loop can't cross instruction patch alignment boundary");

         // (We don't need any mutual exclusion on this patching because the
         // MCC logic ensures only one thread patches a given call site at a
         // time.)

         *(uint16_t *)patchAddr = 0xfeeb;
         patchingFence16(patchAddr);

         patchAddr[2] = (distance>>8) & 0xff;
         patchAddr[3] = (distance>>16) & 0xff;
         patchAddr[4] = (distance>>24) & 0xff;
         patchingFence16(patchAddr);

         *(uint16_t *)patchAddr = 0xe8 | ((distance<<8) & 0xff00);
         }
      }
   return true;
   }

void amd64CodeCacheParameters(int32_t *trampolineSize, OMR::CodeCacheCodeGenCallbacks *callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = TRAMPOLINE_SIZE;
   callBacks->codeCacheConfig = &amd64CodeCacheConfig;
   callBacks->createHelperTrampolines = &amd64CreateHelperTrampolines;
   callBacks->createMethodTrampoline = &amd64CreateMethodTrampoline;
   callBacks->patchTrampoline = &amd64CodePatching;
   callBacks->createCCPreLoadedCode = TR::createCCPreLoadedCode;
   *CCPreLoadedCodeSize = TR::getCCPreLoadedCodeSize();
   *numHelpers = TR_AMD64numRuntimeHelpers;
   }


#endif

#undef TRAMPOLINE_SIZE

#endif /*(TR_TARGET_X86) && (TR_TARGET_64BIT)*/


#if defined(TR_TARGET_X86) && defined(TR_TARGET_32BIT)

void ia32CodeCacheParameters(int32_t *trampolineSize, OMR::CodeCacheCodeGenCallbacks *callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = 0;
   callBacks->codeCacheConfig = NULL;
   callBacks->createHelperTrampolines = NULL;
   callBacks->createMethodTrampoline = NULL;
   callBacks->patchTrampoline = NULL;
   callBacks->createCCPreLoadedCode = TR::createCCPreLoadedCode;
   *CCPreLoadedCodeSize = TR::getCCPreLoadedCodeSize();
   *numHelpers = 0;
   }

#endif /*(TR_TARGET_X86) && (TR_TARGET_32BIT)*/


#if defined(TR_TARGET_ARM)

#define TRAMPOLINE_SIZE         8

#define BRANCH_FORWARD_LIMIT    0x01fffffc
#define BRANCH_BACKWARD_LIMIT   0xfe000000

#if defined(TR_HOST_ARM)
extern void armCodeSync(uint8_t *, uint32_t);
#endif /* TR_HOST_ARM */

void armCodeCacheConfig(int32_t ccSizeInByte, int32_t *numTempTrampolines)
   {
   // ARM method trampoline can be modified in place.
   *numTempTrampolines = 0;
   }

void armCreateHelperTrampolines(void *trampPtr, int32_t numHelpers)
   {
   uint32_t *buffer = (uint32_t *)((uint8_t *)trampPtr + TRAMPOLINE_SIZE);  // Skip the first trampoline for index 0

   for (int32_t i=1; i<numHelpers; i++)
      {
      // LDR  PC, [PC, #-4]
      // DCD  helperAddr
      //
      *buffer = 0xe51ff004;
      buffer += 1;
      *buffer = (intptrj_t)runtimeHelperValue((TR_RuntimeHelper)i);
      buffer += 1;
      }

#if defined(TR_HOST_ARM)
   armCodeSync((uint8_t*)trampPtr, TRAMPOLINE_SIZE * numHelpers);
#endif

// TODO: X-compilation support and tprof support
   }

void armCreateMethodTrampoline(void *trampPtr, void *startPC, void *method)
   {
   uint32_t *buffer = (uint32_t *)trampPtr;
   TR_LinkageInfo *linkInfo = TR_LinkageInfo::get(startPC);
   intptrj_t dispatcher = (intptrj_t)((uint8_t *)startPC + linkInfo->getReservedWord());

   // LDR  PC, [PC, #-4]
   // DCD  dispatcher
   //
   *buffer = 0xe51ff004;
   buffer += 1;
   *buffer = dispatcher;

#if defined(TR_HOST_ARM)
   armCodeSync((uint8_t*)trampPtr, TRAMPOLINE_SIZE);
#endif

// TODO: X-compilation support and tprof support
   }


bool armCodePatching(void *callee, void *callSite, void *currentPC, void *currentTramp, void *newAddrOfCallee, void *extra)
   {
   TR_LinkageInfo *linkInfo = TR_LinkageInfo::get(newAddrOfCallee);
   uint8_t        *entryAddress = (uint8_t *)newAddrOfCallee + linkInfo->getReservedWord();
   intptrj_t       distance;
   int32_t         currentDistance;
   int32_t         branchInstr = *(int32_t *)callSite;
   void           *newTramp;

   distance = entryAddress - (uint8_t *)callSite - 8;
   currentDistance = (branchInstr << 8) >> 6;
   branchInstr &= 0xff000000;

   if (branchInstr != 0xEB000000)
      {
      // This is not a 'bl' instruction -- Don't patch
      return true;
      }

   if (TR::Options::getCmdLineOptions()->getOption(TR_StressTrampolines) || distance>(intptrj_t)BRANCH_FORWARD_LIMIT || distance<(intptrj_t)BRANCH_BACKWARD_LIMIT)
      {
      if (currentPC == newAddrOfCallee)
         {
         newTramp = currentTramp;
         }
      else
         {
         newTramp = mcc_replaceTrampoline(reinterpret_cast<TR_OpaqueMethodBlock *>(callee), callSite, currentTramp, currentPC, newAddrOfCallee, false);
         //TR_ASSERT(newTramp != NULL, "This is an internal error.\n");

         if (currentTramp == NULL)
            {
            armCreateMethodTrampoline(newTramp, newAddrOfCallee, callee);
            }
         else
            {
            *((uint32_t*)currentTramp+1) = (uint32_t)entryAddress;
#if defined(TR_HOST_ARM)
            armCodeSync((uint8_t*)currentTramp+4, 4);
#endif
            }
         }

      distance = (uint8_t *)newTramp - (uint8_t *)callSite - 8;
      }

   if (currentDistance != distance)
      {
      branchInstr |= (distance >> 2) & 0x00ffffff;
      *(int32_t *)callSite = branchInstr;
#if defined(TR_HOST_ARM)
      armCodeSync((uint8_t*)callSite, 4);
#endif
      }

   return true;
   }

void armCodeCacheParameters(int32_t *trampolineSize, void **callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = TRAMPOLINE_SIZE;
   callBacks[0] = (void *)&armCodeCacheConfig;
   callBacks[1] = (void *)&armCreateHelperTrampolines;
   callBacks[2] = (void *)&armCreateMethodTrampoline;
   callBacks[3] = (void *)&armCodePatching;
   callBacks[4] = (void *)0; // CreatePreLoadedCCPreLoadedCode
   *CCPreLoadedCodeSize = 0;
   *numHelpers = TR_ARMnumRuntimeHelpers;
   }

#undef TRAMPOLINE_SIZE
#undef BRANCH_FORWARD_LIMIT
#undef BRANCH_BACKWARD_LIMIT

#endif /* TR_TARGET_ARM */


#if defined(TR_TARGET_ARM64)

void arm64CodeCacheConfig(int32_t ccSizeInByte, int32_t *numTempTrampolines)
   {
   *numTempTrampolines = 0;
   }

void arm64CreateHelperTrampolines(void *trampPtr, int32_t numHelpers)
   {
   uint32_t *buffer = (uint32_t *)((uint8_t *)trampPtr + 16);
   for (int32_t i=1; i<numHelpers; i++)
      {
      *((int32_t *)buffer) = 0x58000110; //LDR R16 PC+8
      buffer += 1; 
      *buffer = 0xD61F0200; //BR R16
      buffer += 1;
      *((intptrj_t *)buffer) = (intptrj_t)runtimeHelperValue((TR_RuntimeHelper)i);
      buffer += 1;
      }
   }

void arm64CreateMethodTrampoline(void *trampPtr, void *startPC, void *method)
   {
   uint32_t *buffer = (uint32_t *)trampPtr;
   TR_LinkageInfo *linkInfo = TR_LinkageInfo::get(startPC);
   intptrj_t dispatcher = (intptrj_t)((uint8_t *)startPC + linkInfo->getReservedWord());

   *buffer = 0x58000110; //LDR R16 PC+8
   buffer += 1; 
   *buffer = 0xD61F0200; //BR R16
   buffer += 1;
   *((intptrj_t *)buffer) = dispatcher;
   }

bool arm64CodePatching(void *callee, void *callSite, void *currentPC, void *currentTramp, void *newAddrOfCallee, void *extra)
   {
   TR_LinkageInfo *linkInfo = TR_LinkageInfo::get(newAddrOfCallee);
   uint8_t        *entryAddress = (uint8_t *)newAddrOfCallee + linkInfo->getReservedWord();
   intptrj_t       distance;
   int32_t         currentDistance;
   int32_t         branchInstr = *(int32_t *)callSite;
   void           *newTramp;

   distance = entryAddress - (uint8_t *)callSite;
   currentDistance = (branchInstr << 6) >> 4;
   branchInstr &= 0xfc000000;
   
   if (branchInstr != 0x94000000)
      {
      // This is not a 'bl' instruction -- Don't patch
      return true;
      }

   if (TR::Options::getCmdLineOptions()->getOption(TR_StressTrampolines)
            || distance>(intptrj_t)TR::Compiler->target.cpu.maxUnconditionalBranchImmediateForwardOffset() 
            || distance<(intptrj_t)TR::Compiler->target.cpu.maxUnconditionalBranchImmediateBackwardOffset()
   )  {
      if (currentPC == newAddrOfCallee)
         {
         newTramp = currentTramp; 
         }
      else
         {
         newTramp = mcc_replaceTrampoline(reinterpret_cast<TR_OpaqueMethodBlock *>(callee), callSite, currentTramp, currentPC, newAddrOfCallee, false);
         TR_ASSERT_FATAL(newTramp != NULL, "Internal error: Could not replace trampoline.\n");

         if (currentTramp == NULL)
            {
            arm64CreateMethodTrampoline(newTramp, newAddrOfCallee, callee);
            }
         else
            {
            *((uint32_t*)currentTramp+1) = (uint32_t)entryAddress;
            }
         }

      distance = (uint8_t *)newTramp - (uint8_t *)callSite;
      }

   if (currentDistance != distance)
      {
      branchInstr |= (distance >> 2) & 0x03ffffff;
      *(int32_t *)callSite = branchInstr;
      }

   return true;
   }

void arm64CodeCacheParameters(int32_t *trampolineSize, void **callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = 16;
   callBacks[0] = (void *)&arm64CodeCacheConfig;
   callBacks[1] = (void *)&arm64CreateHelperTrampolines;
   callBacks[2] = (void *)&arm64CreateMethodTrampoline;
   callBacks[3] = (void *)&arm64CodePatching;
   callBacks[4] = (void *)0; // CreatePreLoadedCCPreLoadedCode
   *CCPreLoadedCodeSize = 0;
   *numHelpers = TR_ARM64numRuntimeHelpers;
   }

#endif /* TR_TARGET_ARM64 */


#if defined(J9ZOS390) && defined(TR_TARGET_S390) && !defined(TR_TARGET_64BIT)

//-----------------------------------------------------------------------------
//   zOS 31 Bit - "plain" Multi Code Cache Support
//-----------------------------------------------------------------------------

// ZOS390 MCC Support: minimum hookup without callbacks.
void s390zOS31CodeCacheParameters(int32_t *trampolineSize, void **callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = 0;
   callBacks[0] = NULL;
   callBacks[1] = NULL;
   callBacks[2] = NULL;
   callBacks[3] = NULL;
   callBacks[4] = (void *)0; // CreatePreLoadedCCPreLoadedCode
   *CCPreLoadedCodeSize = 0;
   *numHelpers = 0;
   }

#elif defined(J9ZOS390) && defined(TR_TARGET_64BIT) && defined(TR_TARGET_S390)

//-----------------------------------------------------------------------------
//   zOS 64 Bit - "plain" Multi Code Cache Support
//-----------------------------------------------------------------------------

// Size of a Trampoline (Padded to be 8-byte aligned)
#define TRAMPOLINE_SIZE 24
#define CALL_INSTR_LENGTH 0

// Atomic Storage of a 4 byte value - Picbuilder.m4
extern "C" void _Store4(int32_t * addr, uint32_t newData);
extern "C" void _Store8(intptrj_t * addr, uintptrj_t newData);


//Note method trampolines no longer used
void s390zOS64CodeCacheConfig(int32_t ccSizeInByte, int32_t *numTempTrampolines)
   {
   // The method trampolines used by zOS can be modified in place.
   // Only modification of the data constant after the BCR instruction is required.
   // This DC is guaranteed to be aligned for atomic ST.
   *numTempTrampolines = 0;
   }

// zOS64 Create Trampolines to Runtime Helper Routines.
void s390zOS64CreateHelperTrampoline(void *trampPtr, int32_t numHelpers)
   {
   uint8_t *bufferStart = (uint8_t *)trampPtr, *buffer;

   // Get the Entry Pointer (should be r15 for zOS64).
   uint16_t rEP;

   // Create a Trampoline for each runtime helper.
   for (int32_t i = 1; i < numHelpers; i++)
      {
      // Get the helper address
      intptrj_t helperAddr = (intptrj_t)runtimeHelperValue((TR_RuntimeHelper)i);

      // Skip the first trampoline for index 0
      bufferStart += TRAMPOLINE_SIZE;
      buffer = bufferStart;

      // In C Helper linkage GPR15 is a preserved register and hence is not evacuated at helper call sites. Because
      // we cannot clobber a preserved register we remap the entry point register for C Helper calls to GPR6 which
      // is also a preserved register but will inevitably be clobbered by the linkage glue.
      if (runtimeHelperLinkage((TR_RuntimeHelper)i) == TR_CHelper)
         rEP = 6;
      else
         rEP = 15;

      static bool enableIIHF = (feGetEnv("TR_IIHF") != NULL);

      if (!enableIIHF)
         {
         // Trampoline Code:
         // zOS Linkage rEP = r15.
         // 0d40           BASR  rEP, 0
         // e340400e04     LG    rEP, 14(,rEP)
         // 07f4           BCR   rEP
         // 6-bytes padding
         //                DC    helperAddr

         // BASR rEP, 0;
         *(int16_t *)buffer = 0x0d00 + (rEP << 4);
         buffer += sizeof(int16_t);

         // LG rEP, 14(,rEP)
         *(int32_t *)buffer = 0xe300000e + (rEP << 12) + (rEP << 20);
         buffer += sizeof(int32_t);
         *(int16_t *)buffer = 0x0004;
         buffer += sizeof(int16_t);
         // BCR rEP
         *(int16_t *)buffer = 0x07f0 + rEP;
         buffer += sizeof(int16_t);

         // 6-byte Padding.
         *(int32_t *)buffer = 0x00000000;
         buffer += sizeof(int32_t);
         *(int16_t *)buffer = 0x0000;
         buffer += sizeof(int16_t);

         // DC mAddr
         *(intptrj_t *)buffer = helperAddr;
         buffer += sizeof(intptrj_t);
         }
      else
         {
         //Alternative Trampoline code
         // IIHF rEP addr
         // IILF rEP addr
         // BRC  rEP

         uint32_t low = (uint32_t)helperAddr;
         uint32_t high = (uint32_t)(helperAddr >> 32);

         // IIHF rEP, mAddr;
         *(int16_t *)buffer = 0xC008 + (rEP << 4);
         buffer += sizeof(int16_t);
         *(int32_t *)buffer = 0x00000000 + high;
         buffer += sizeof(int32_t);

         // IILF rEP, mAddr;
         *(int16_t *)buffer = 0xC009 + (rEP << 4);
         buffer += sizeof(int16_t);
         *(int32_t *)buffer = 0x00000000 + low;
         buffer += sizeof(int32_t);

         // BCR rEP
         *(int16_t *)buffer = 0x07f0 + rEP;
         buffer += sizeof(int16_t);
         }
      }
   }

// zOS64 Create Method Trampoline
void s390zOS64CreateMethodTrampoline(void *trampPtr, void *startPC, void *method)
   {
   uint8_t     *buffer = (uint8_t *)trampPtr;
   // Get the Entry Pointer (should be r15 for zOS64).
   uint16_t rEP = 15;
   TR_LinkageInfo *linkInfo = TR_LinkageInfo::get(startPC);
   intptrj_t dispatcher = (intptrj_t)((uint8_t *)startPC + linkInfo->getReservedWord());

   static bool enableIIHF = (feGetEnv("TR_IIHF") != NULL);

   if (!enableIIHF)
      {
      // Trampoline Code:
      // zOS Linkage rEP = r4.
      // 0d40           BASR  rEP, 0
      // e340400804     LG    rEP, 14(,rEP)
      // 07f4           BCR   rEP
      // 6-bytes padding to align DC address.
      //                DC    dispatcher

      // BASR rEP, 0;
      *(int16_t *)buffer = 0x0d00 + (rEP << 4);
      buffer += sizeof(int16_t);

      // LG rEP, 14(,rEP)
      *(int32_t *)buffer = 0xe300000e + (rEP << 12) + (rEP << 20);
      buffer += sizeof(int32_t);
      *(int16_t *)buffer = 0x0004;
      buffer += sizeof(int16_t);

      // BCR rEP
      *(int16_t *)buffer = 0x07f0 + rEP;
      buffer += sizeof(int16_t);

      // 6-byte Padding.
      *(int32_t *)buffer = 0x00000000;
      buffer += sizeof(int32_t);
      *(int16_t *)buffer = 0x0000;
      buffer += sizeof(int16_t);

      // DC mAddr
      *(intptrj_t *)buffer = dispatcher;
      buffer += sizeof(intptrj_t);
      }
   else
      {
      //Alternative Trampoline code
      // IIHF rEP addr
      // IILF rEP addr
      // BRC  rEP

      uint32_t low = (uint32_t) dispatcher;
      uint32_t high = (uint32_t) (dispatcher >> 32);

      // IIHF rEP, mAddr;
      *(int16_t *)buffer = 0xC008 + (rEP << 4);
      buffer += sizeof(int16_t);
      *(int32_t *)buffer = 0x00000000 + high;
      buffer += sizeof(int32_t);

      // IILF rEP, mAddr;
      *(int16_t *)buffer = 0xC009 + (rEP << 4);
      buffer += sizeof(int16_t);
      *(int32_t *)buffer = 0x00000000 + low;
      buffer += sizeof(int32_t);

      // BCR rEP
      *(int16_t *)buffer = 0x07f0 + rEP;
      buffer += sizeof(int16_t);
      }
   }

// zOS64 Code Patching.
bool s390zOS64CodePatching(void *method, void *callSite, void *currentPC, void *currentTramp, void *newPC, void *extra)
   {
   TR_LinkageInfo *linkInfo = TR_LinkageInfo::get(newPC);
   // The location of the method call branch.
   uint8_t        *entryAddress = (uint8_t *)newPC + linkInfo->getReservedWord();
   // The location of the callsite.
   uint8_t        *patchAddress = (uint8_t *)callSite;
   // Distance between the call site and branch target.
   intptrj_t      distance = (intptrj_t)entryAddress - (intptrj_t)patchAddress - CALL_INSTR_LENGTH;
   // Current Displacement of call site instruction
   int32_t        currentDistance = *(int32_t *)(patchAddress + 2);

   #ifdef CODE_CACHE_TRAMPOLINE_DEBUG
   fprintf(stderr, "Patching.\n");
   fprintf(stderr, "J9Method: %p, CallSite: %p, currentPC: %p, currentTramp: %p, newPC: %p, entryAddress: %p\n",method, callSite, currentPC, currentTramp, newPC, entryAddress);
   fprintf(stderr, "Checking Trampoline: Distance - %ld\n",distance);
   #endif

   //#define CHECK_32BIT_TRAMPOLINE_RANGE(x,rip)  (((intptrj_t)(x) == (intptrj_t)(rip) + (int32_t)((intptrj_t)(x) - (intptrj_t)(rip))) && (x % 2 == 0))

   // call instruction should be BASRL rRA,Imm  with immediate field aligned.
   if (TR::Options::getCmdLineOptions()->getOption(TR_StressTrampolines) || !CHECK_32BIT_TRAMPOLINE_RANGE(distance,0))
      {
      // Check if the call already jumps to our current trampoline.
      // If so, do nothing.
      if (currentPC == newPC)
         {
         distance = (uint8_t *)currentTramp - (uint8_t *)patchAddress - CALL_INSTR_LENGTH;
         }
      else
         {
         // Replace the current trampoline
         void *newTramp = mcc_replaceTrampoline(reinterpret_cast<TR_OpaqueMethodBlock *>(method), callSite, currentTramp, currentPC, newPC, false);
         //TR_ASSERT(newTramp != NULL  && (currentTramp == NULL || currentTramp == newTramp), "An Internal Error."); // KEN
         distance = (uint8_t *)newTramp - (uint8_t *)patchAddress - CALL_INSTR_LENGTH;

         if (currentTramp == NULL)
            {
            // Create a new Trampoline.
            s390zOS64CreateMethodTrampoline(newTramp, newPC, method);
            }
         else
            {
            // Patch the existing trampoline.
            // Should not require Self-loops in patching since trampolines addresses
            // should be aligned by 8-bytes, and STG is atomic.
            _Store8((intptrj_t *)((uint32_t *)currentTramp + 4), (intptrj_t)entryAddress);
            }
         }
      }
   // KEN TR_ASSERT(CHECK_32BIT_TRAMPOLINE_RANGE(distance,0), "Local trampoline should be directly reachable. \n");

   // Modify the Call Instruction!
   // All Call instructions should have immediate field 32 bit aligned!
   if (currentDistance != distance)
      {
      //*(uint32_t *)(patchAddress + 2) = distance/2;
      _Store4((int32_t *)(patchAddress + 2), distance / 2);
      }
   return true;
   }


// ZOS390 MCC Support: minimum hookup without callbacks.
// Default executable space is limited to 2GB: No Trampolines required.
// Can increase executable space by enabling RMODE64, Trampolines will be required
void s390zOS64CodeCacheParameters(int32_t *trampolineSize, void **callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   if (TR::Options::getCmdLineOptions()->getOption(TR_EnableRMODE64))
      {
      *trampolineSize = TRAMPOLINE_SIZE;
      callBacks[0] = (void *)&s390zOS64CodeCacheConfig;
      callBacks[1] = (void *)&s390zOS64CreateHelperTrampoline;
      callBacks[2] = (void *)&s390zOS64CreateMethodTrampoline;
      callBacks[3] = (void *)&s390zOS64CodePatching;
      callBacks[4] = (void *)0; // CreatePreLoadedCCPreLoadedCode
      *CCPreLoadedCodeSize = 0;
      *numHelpers = TR_S390numRuntimeHelpers;
      }
   else
      {
      *trampolineSize = 0;
      callBacks[0] = NULL;
      callBacks[1] = NULL;
      callBacks[2] = NULL;
      callBacks[3] = NULL;
      callBacks[4] = (void *)0; // CreatePreLoadedCCPreLoadedCode
      *CCPreLoadedCodeSize = 0;
      *numHelpers = 0;
      }
   }

#undef TRAMPOLINE_SIZE

#elif !defined(J9ZOS390) && defined(TR_TARGET_S390) && !defined(TR_TARGET_64BIT)

//-----------------------------------------------------------------------------
//   zLinux 31 Bit - "plain" Multi Code Cache Support
//-----------------------------------------------------------------------------

// ZLinux390 MCC Support: minimum hookup without callbacks.
void s390zLinux31CodeCacheParameters(int32_t *trampolineSize, void **callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = 0;
   callBacks[0] = NULL;
   callBacks[1] = NULL;
   callBacks[2] = NULL;
   callBacks[3] = NULL;
   callBacks[4] = (void *)0; // CreatePreLoadedCCPreLoadedCode
   *CCPreLoadedCodeSize = 0;
   *numHelpers = 0;
   }

#elif defined(TR_TARGET_S390) && defined(TR_TARGET_64BIT) && !defined(J9ZOS390)

// Size of a Trampoline (Padded to be 8-byte aligned)
#define TRAMPOLINE_SIZE 24
#define CALL_INSTR_LENGTH 0

// Atomic Storage of a 4 byte value - Picbuilder.m4
extern "C" void _Store4(int32_t * addr, uint32_t newData);
extern "C" void _Store8(intptrj_t * addr, uintptrj_t newData);

// zLinux64 Configuration of Code Cache.
void s390zLinux64CodeCacheConfig(int32_t ccSizeInByte, int32_t *numTempTrampolines)
   {
   // The method trampolines used by zLinux can be modified in place.
   // Only modification of the data constant after the BCR instruction is required.
   // This DC is guaranteed to be aligned for atomic ST.
   *numTempTrampolines = 0;
   }

// zLinux64 Create Trampolines to Runtime Helper Routines.
void s390zLinux64CreateHelperTrampoline(void *trampPtr, int32_t numHelpers)
   {
   uint8_t *bufferStart = (uint8_t *)trampPtr, *buffer;
   // Get the Entry Pointer (should be r4 for zLinux64).
   uint16_t rEP; // Joran TODO: use getEPRegNum instead.
   // Create a Trampoline for each runtime helper.
   for (int32_t i = 1; i < numHelpers; i++)
      {

      // Get the helper address
      intptrj_t helperAddr = (intptrj_t)runtimeHelperValue((TR_RuntimeHelper)i);

      // Skip the first trampoline for index 0
      bufferStart += TRAMPOLINE_SIZE;
      buffer = bufferStart;
      // If a helper with C Linkage is called, it expects argument in GPR4 and we can not use GPR4 as entry point register
      // For this scenarios, we will be using GPR1 which is volatile so already being freed up by helper call
      if (runtimeHelperLinkage((TR_RuntimeHelper)i) == TR_CHelper)
         rEP = 1;
      else
         rEP = 4;

      static bool enableIIHF = (feGetEnv("TR_IIHF") != NULL);

      if (enableIIHF)
         {
         //Alternative Trampoline code
         // IIHF rEP addr
         // IILF rEP addr
         // BRC  rEP

         uint32_t low = (uint32_t)helperAddr;
         uint32_t high = (uint32_t)(helperAddr >> 32);

         // IIHF rEP, mAddr;
         *(int16_t *)buffer = 0xC008 + (rEP << 4);
         buffer += sizeof(int16_t);
         *(int32_t *)buffer = 0x00000000 + high;
         buffer += sizeof(int32_t);

         // IILF rEP, mAddr;
         *(int16_t *)buffer = 0xC009 + (rEP << 4);
         buffer += sizeof(int16_t);
         *(int32_t *)buffer = 0x00000000 + low;
         buffer += sizeof(int32_t);

         // BCR rEP
         *(int16_t *)buffer = 0x07f0 + rEP;
         buffer += sizeof(int16_t);
         }
      else
         {
         // Trampoline Code:
         // zLinux Linkage rEP = r4.
         // 0d40           BASR  rEP, 0
         // e340400e04     LG    rEP, 14(,rEP)
         // 07f4           BCR   rEP
         // 6-bytes padding
         //                DC    helperAddr

         // BASR rEP, 0;
         *(int16_t *)buffer = 0x0d00 + (rEP << 4);
         buffer += sizeof(int16_t);

         // LG rEP, 14(,rEP)
         *(int32_t *)buffer = 0xe300000e + (rEP << 12) + (rEP << 20);
         buffer += sizeof(int32_t);
         *(int16_t *)buffer = 0x0004;
         buffer += sizeof(int16_t);

         // BCR rEP
         *(int16_t *)buffer = 0x07f0 + rEP;
         buffer += sizeof(int16_t);

         // 6-byte Padding.
         *(int32_t *)buffer = 0x00000000;
         buffer += sizeof(int32_t);
         *(int16_t *)buffer = 0x0000;
         buffer += sizeof(int16_t);

         // DC mAddr
         *(intptrj_t *)buffer = helperAddr;
         buffer += sizeof(intptrj_t);
         }
      }
   }

// zLinux64 Create Method Trampoline
void s390zLinux64CreateMethodTrampoline(void *trampPtr, void *startPC, void *method)
   {
   uint8_t     *buffer = (uint8_t *)trampPtr;
   // Get the Entry Pointer (should be r4 for zLinux64).
   uint16_t rEP = 4;  // Joran TODO: useEPRegNum instead.
   TR_LinkageInfo *linkInfo = TR_LinkageInfo::get(startPC);
   intptrj_t dispatcher = (intptrj_t)((uint8_t *) startPC + linkInfo->getReservedWord());

   static bool enableIIHF = (feGetEnv("TR_IIHF") != NULL);

   if (enableIIHF)
      {
      //Alternative Trampoline code
      // IIHF rEP addr
      // IILF rEP addr
      // BRC  rEP

      uint32_t low = (uint32_t) dispatcher;
      uint32_t high = (uint32_t) (dispatcher >> 32);

      // IIHF rEP, mAddr;
      *(int16_t *)buffer = 0xC008 + (rEP << 4);
      buffer += sizeof(int16_t);
      *(int32_t *)buffer = 0x00000000 + high;
      buffer += sizeof(int32_t);

      // IILF rEP, mAddr;
      *(int16_t *)buffer = 0xC009 + (rEP << 4);
      buffer += sizeof(int16_t);
      *(int32_t *)buffer = 0x00000000 + low;
      buffer += sizeof(int32_t);
      }
   else
      {
      // Trampoline Code:
      // zLinux Linkage rEP = r4.
      // 0d40           BASR  rEP, 0
      // e340400804     LG    rEP, 14(,rEP)
      // 07f4           BCR   rEP
      // 6-bytes padding to align DC address.
      //                DC    dispatcher

      // BASR rEP, 0;
      *(int16_t *)buffer = 0x0d00 + (rEP << 4);
      buffer += sizeof(int16_t);

      // LG rEP, 14(,rEP)
      *(int32_t *)buffer = 0xe300000e + (rEP << 12) + (rEP << 20);
      buffer += sizeof(int32_t);
      *(int16_t *)buffer = 0x0004;
      buffer += sizeof(int16_t);

      // BCR rEP
      *(int16_t *)buffer = 0x07f0 + rEP;
      buffer += sizeof(int16_t);

      // 6-byte Padding.
      *(int32_t *)buffer = 0x00000000;
      buffer += sizeof(int32_t);
      *(int16_t *)buffer = 0x0000;
      buffer += sizeof(int16_t);

      // DC mAddr
      *(intptrj_t *)buffer = dispatcher;
      buffer += sizeof(intptrj_t);
      }
   }

// zLinux64 Code Patching.
bool s390zLinux64CodePatching (void *method, void *callSite, void *currentPC, void *currentTramp, void *newPC, void *extra)
   {
   TR_LinkageInfo *linkInfo = TR_LinkageInfo::get(newPC);
   // The location of the method call branch.
   uint8_t        *entryAddress = (uint8_t *)newPC + linkInfo->getReservedWord();
   // The location of the callsite.
   uint8_t        *patchAddress = (uint8_t *)callSite;
   // Distance between the call site and branch target.
   intptrj_t      distance = (intptrj_t)entryAddress - (intptrj_t)patchAddress - CALL_INSTR_LENGTH;
   // Current Displacement of call site instruction
   int32_t        currentDistance = *(int32_t *)(patchAddress + 2);

#ifdef CODE_CACHE_TRAMPOLINE_DEBUG
   fprintf(stderr, "Patching.\n");
   fprintf(stderr, "J9Method: %p, CallSite: %p, currentPC: %p, currentTramp: %p, newPC: %p, entryAddress: %p\n",method, callSite, currentPC, currentTramp, newPC, entryAddress);
   fprintf(stderr, "Checking Trampoline: Distance - %ld\n",distance);
#endif

//#define CHECK_32BIT_TRAMPOLINE_RANGE(x,rip)  (((intptrj_t)(x) == (intptrj_t)(rip) + (int32_t)((intptrj_t)(x) - (intptrj_t)(rip))) && (x % 2 == 0))

   // call instruction should be BASRL rRA,Imm  with immediate field aligned.
   if (TR::Options::getCmdLineOptions()->getOption(TR_StressTrampolines) || !CHECK_32BIT_TRAMPOLINE_RANGE(distance,0))
      {
      // Check if the call already jumps to our current trampoline.
      // If so, do nothing.
      if (currentPC == newPC)
         {
         distance = (uint8_t *)currentTramp - (uint8_t *)patchAddress - CALL_INSTR_LENGTH;
         }
      else
         {
         // Replace the current trampoline
         void *newTramp = mcc_replaceTrampoline(reinterpret_cast<TR_OpaqueMethodBlock *>(method), callSite, currentTramp, currentPC, newPC, false);
         //TR_ASSERT(newTramp != NULL  && (currentTramp == NULL || currentTramp == newTramp), "An Internal Error."); // KEN
         distance = (uint8_t *)newTramp - (uint8_t *)patchAddress - CALL_INSTR_LENGTH;

         if (currentTramp == NULL)
            {
            // Create a new Trampoline.
            s390zLinux64CreateMethodTrampoline(newTramp, newPC, method);
            }
         else
            {  // Patch the existing trampoline.
            // Should not require Self-loops in patching since trampolines addresses
            // should be aligned by 8-bytes, and STG is atomic.
            _Store8((intptrj_t *)((uint32_t *)currentTramp + 4),(intptrj_t)entryAddress);
            }
         }
      }
      // KEN TR_ASSERT(CHECK_32BIT_TRAMPOLINE_RANGE(distance,0), "Local trampoline should be directly reachable. \n");

      // Modify the Call Instruction!
      // All Call instructions should have immediate field 32 bit aligned!
      if (currentDistance != distance)
         {
         //*(uint32_t *)(patchAddress + 2) = distance/2;
         _Store4((int32_t *)(patchAddress + 2), distance /2);
         }
      return true;
   }

// zLinux64 MCC Support: Maximum Hookup.
void s390zLinux64CodeCacheParameters(int32_t *trampolineSize, void **callBacks, int32_t *numHelpers, int32_t* CCPreLoadedCodeSize)
   {
   *trampolineSize = TRAMPOLINE_SIZE;
   // Call Backs for Code Gen.
   callBacks[0] = (void *)&s390zLinux64CodeCacheConfig;
   callBacks[1] = (void *)&s390zLinux64CreateHelperTrampoline;
   callBacks[2] = (void *)&s390zLinux64CreateMethodTrampoline;
   callBacks[3] = (void *)&s390zLinux64CodePatching;
   callBacks[4] = (void *)0; // CreatePreLoadedCCPreLoadedCode
   *CCPreLoadedCodeSize = 0;
   *numHelpers = TR_S390numRuntimeHelpers;
   }

#endif

void setupCodeCacheParameters(int32_t *trampolineSize, OMR::CodeCacheCodeGenCallbacks *callBacks, int32_t * numHelpers, int32_t *CCPreLoadedCodeSize)
   {
#if defined(TR_TARGET_POWER)
   if (TR::Compiler->target.cpu.isPower())
      {
      ppcCodeCacheParameters(trampolineSize, (void **)callBacks, numHelpers, CCPreLoadedCodeSize);
      return;
      }
#endif

#if defined(TR_TARGET_X86) && defined(TR_TARGET_32BIT)
   if (TR::Compiler->target.cpu.isI386())
      {
      ia32CodeCacheParameters(trampolineSize, callBacks, numHelpers, CCPreLoadedCodeSize);
      return;
      }
#endif

#if defined(TR_TARGET_X86) && defined(TR_TARGET_64BIT)
   if (TR::Compiler->target.cpu.isAMD64())
      {
      amd64CodeCacheParameters(trampolineSize, callBacks, numHelpers, CCPreLoadedCodeSize);
      return;
      }
#endif

#if defined(TR_TARGET_ARM)
   if (TR::Compiler->target.cpu.isARM())
      {
      armCodeCacheParameters(trampolineSize, (void **)callBacks, numHelpers, CCPreLoadedCodeSize);
      return;
      }
#endif

#if defined(TR_TARGET_ARM64)
   if (TR::Compiler->target.cpu.isARM64())
      {
      arm64CodeCacheParameters(trampolineSize, (void **)callBacks, numHelpers, CCPreLoadedCodeSize);
      return;
      }
#endif

#if defined(TR_TARGET_S390) && !defined(TR_TARGET_64BIT) && defined(J9ZOS390)
   // zOS 31 code cache support.
   if (TR::Compiler->target.cpu.isZ())
      {
      s390zOS31CodeCacheParameters(trampolineSize, (void **)callBacks, numHelpers, CCPreLoadedCodeSize);
      }
#endif

#if defined(TR_TARGET_S390) && defined(TR_TARGET_64BIT) && defined(J9ZOS390)
   // zOS 64 code cache support.
   if (TR::Compiler->target.cpu.isZ())
      {
      s390zOS64CodeCacheParameters(trampolineSize, (void **)callBacks, numHelpers, CCPreLoadedCodeSize);
      }
#endif

#if defined(TR_TARGET_S390) && !defined(TR_TARGET_64BIT) && !defined(J9ZOS390)
   // zLinux 31 code cache support.
   if (TR::Compiler->target.cpu.isZ())
      {
      s390zLinux31CodeCacheParameters(trampolineSize, (void **)callBacks, numHelpers, CCPreLoadedCodeSize);
      }
#endif

#if defined(TR_TARGET_S390) && defined(TR_TARGET_64BIT) && !defined(J9ZOS390)
   // zLinux 64 code cache support.
   if (TR::Compiler->target.cpu.isZ())
      {
      s390zLinux64CodeCacheParameters(trampolineSize, (void **)callBacks, numHelpers, CCPreLoadedCodeSize);
      }
#endif
   }
