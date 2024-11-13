/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <ucontext.h>
#include <stdint.h>

#include "j9protos.h"
#include "j9cfg.h"

#ifdef LINUX

#include "../omr/port/linuxs390/omrsignal_context.h"

#include "infra/Bit.hpp"

class InstEmulator
   {
   public:
      static InstEmulator *decode(uint8_t *pc);
      virtual void emulate(mcontext_t *cpu) {}
   };

class LXAEmulator : public InstEmulator
   {
   private:
      uint8_t r1, x2, b2;
      uint32_t dx2;
      uint8_t shift;
      bool isLogical;
   public:
      LXAEmulator(uint8_t *start);
      virtual void emulate(mcontext_t *cpu);
   };

LXAEmulator::LXAEmulator(uint8_t *start)
   {
   r1 = (start[1] & 0xF0) >> 4;
   x2 = start[1] & 0x0F;
   b2 = (start[2] & 0xF0) >> 4;
   uint32_t dxl2 = (uint32_t) ((*(uint16_t *)(start+2)) & 0x0FFF);
   uint32_t dxh2 = start[4];
   dx2 = dxl2 | (dxh2 << 12);
   shift = (start[5] & 0xE) >> 1;
   isLogical = (start[5] & 1) == 1;
   }

void LXAEmulator::emulate(mcontext_t *cpu)
   {
   int64_t addr;
   int32_t tmp = (int32_t) dx2;

   // sign extend immediate
   if (tmp & 0x80000)
      {
      tmp |= 0xFFF00000;
      }

   if (x2 != 0)
      {
      tmp += cpu->gregs[x2];
      }

   if (isLogical)
      {
      addr = (int64_t)(uint32_t) tmp;
      }
   else
      {
      addr = (int64_t) tmp;
      }

   addr <<= shift;
   if (b2 != 0)
      {
      addr += cpu->gregs[b2];
      }
   cpu->gregs[r1] = addr;
   }

class BDEPGEmulator : public InstEmulator
   {
   private:
      uint8_t r1, r2, r3;
   public:
      BDEPGEmulator(uint8_t *start);
      virtual void emulate(mcontext_t *cpu);
   };

BDEPGEmulator::BDEPGEmulator(uint8_t *start)
   {
   r1 = (start[3] & 0xF0) >> 4;
   r2 = start[3] & 0x0F;
   r3 = (start[2] & 0xF0) >> 4;
   }

void BDEPGEmulator::emulate(mcontext_t *cpu)
   {
   uint64_t val = cpu->gregs[r2];
   uint64_t mask = cpu->gregs[r3];
   uint64_t res = 0;

   for (int n = 0; mask; n++)
      {
      if (mask & (1ULL << 63))
         {
         res |= (val & (1ULL << 63)) >> n;
         val <<= 1;
         }
      mask <<= 1;
      }

   cpu->gregs[r1] = res;
   }

class BEXTGEmulator : public InstEmulator
   {
   private:
      uint8_t r1, r2, r3;
   public:
      BEXTGEmulator(uint8_t *start);
      virtual void emulate(mcontext_t *cpu);
   };

BEXTGEmulator::BEXTGEmulator(uint8_t *start)
   {
   r1 = (start[3] & 0xF0) >> 4;
   r2 = start[3] & 0x0F;
   r3 = (start[2] & 0xF0) >> 4;
   }

void BEXTGEmulator::emulate(mcontext_t *cpu)
   {
   uint64_t val = cpu->gregs[r2];
   uint64_t mask = cpu->gregs[r3];
   uint64_t res = 0;

   for (int k = 0; mask; mask <<= 1, val <<= 1)
      {
      if (mask & (1ULL << 63))
         {
         res |= (val & (1ULL << 63)) >> k;
         k++;
         }
      }

   cpu->gregs[r1] = res;
   }

class CLZGEmulator : public InstEmulator
   {
   private:
      uint8_t r1, r2;
   public:
      CLZGEmulator(uint8_t *start);
      virtual void emulate(mcontext_t *cpu);
   };

CLZGEmulator::CLZGEmulator(uint8_t *start)
   {
   r1 = (start[3] & 0xF0) >> 4;
   r2 = start[3] & 0x0F;
   }

void CLZGEmulator::emulate(mcontext_t *cpu)
   {
   cpu->gregs[r1] = (uint64_t)leadingZeroes(cpu->gregs[r2]);
   }

class CTZGEmulator : public InstEmulator
   {
   private:
      uint8_t r1, r2;
   public:
      CTZGEmulator(uint8_t *start);
      virtual void emulate(mcontext_t *cpu);
   };

CTZGEmulator::CTZGEmulator(uint8_t *start)
   {
   r1 = (start[3] & 0xF0) >> 4;
   r2 = start[3] & 0x0F;
   }

void CTZGEmulator::emulate(mcontext_t *cpu)
   {
   cpu->gregs[r1] = (uint64_t)trailingZeroes(cpu->gregs[r2]);
   }

InstEmulator *InstEmulator::decode(uint8_t *pc)
   {
   // Checking the optcode in the first byte and last byte of the instruction to
   // see if it is LXA/LLXA instructions (Op-codes '0xE360' to '0xE369')
   if ((pc[-6] == 0xE3) && ((pc[-1] & 0xF0) == 0x60) && ((pc[-1] & 0x0F) < 10))
      {
      return new LXAEmulator(pc-6);
      }
   else if (*(uint16_t*)(pc-4) == 0xB96D)
      {
      return new BDEPGEmulator(pc-4);
      }
   else if (*(uint16_t*)(pc-4) == 0xB96C)
      {
      return new BEXTGEmulator(pc-4);
      }
   else if (*(uint16_t*)(pc-4) == 0xB968)
      {
      return new CLZGEmulator(pc-4);
      }
   else if (*(uint16_t*)(pc-4) == 0xB969)
      {
      return new CTZGEmulator(pc-4);
      }

   return NULL;
   }

#endif /* LINUX */

extern "C"
int jitS390Emulation(J9VMThread* vmThread, void* sigInfo)
   {
#ifdef LINUX
   OMRUnixSignalInfo *unixSigInfo = (OMRUnixSignalInfo*) sigInfo;

   uint8_t *pc = (uint8_t*) unixSigInfo->platformSignalInfo.context->uc_mcontext.psw.addr;

   InstEmulator *inst = InstEmulator::decode(pc);
   if (inst != NULL)
      {
      inst->emulate(&unixSigInfo->platformSignalInfo.context->uc_mcontext);
      delete inst;
      return 0;
      }
#endif /* LINUX */

   return -1;
   }
