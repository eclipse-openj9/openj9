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

#include <stdint.h>
#include <ucontext.h>

#include "j9protos.h"
#include "j9cfg.h"

#ifdef LINUX

#include "../omr/port/linuxppc/omrsignal_context.h"

#ifdef __GNUC__
#define trailingZeroes(x) __builtin_ctzl(x)
#elif defined(__IBMC__) || defined(__ibmxl__)
#define trailingZeroes(x) __cnttz8(x)
#endif

typedef enum p9emu_insn_id
   {
   /* These two are not P9 instructions,
    * but will generate SIGILL if they access P9 SPRs,
    * so we can still handle them to emulate the new SPRs.
    */
   P9EMU_MFSPR,
   P9EMU_MTSPR,
   /* This is not a P9 instruction, but is required to emulate ldmx. */
   P9EMU_RFEBB,

   P9EMU_ADDPCIS,
   //P9EMU_BCDCFN_R,
   //P9EMU_BCDCFSQ_R,
   //P9EMU_BCDCFZ_R,
   //P9EMU_BCDCPSGN_R,
   //P9EMU_BCDCTN_R,
   //P9EMU_BCDCTSQ_R,
   //P9EMU_BCDCTZ_R,
   //P9EMU_BCDS_R,
   //P9EMU_BCDSETSGN_R,
   //P9EMU_BCDSR_R,
   //P9EMU_BCDTRUNC_R,
   //P9EMU_BCDUS_R,
   //P9EMU_BCDUTRUNC_R,
#ifdef TR_HOST_64BIT
   P9EMU_CMPEQB,
#endif
   P9EMU_CMPRB,
   //P9EMU_CNTTZD,
   //P9EMU_CNTTZD_R,
   //P9EMU_CNTTZW,
   //P9EMU_CNTTZW_R,
   //P9EMU_COPY,
   //P9EMU_CP_ABORT,
   //P9EMU_DARN,
   //P9EMU_DTSTSFI,
   //P9EMU_DTSTSFIQ,
   P9EMU_EXTSWSLI,
   P9EMU_EXTSWSLI_R,
   //P9EMU_LDAT,
   P9EMU_LDMX,
   //P9EMU_LWAT,
   //P9EMU_LXSD,
   //P9EMU_LXSIBZX,
   //P9EMU_LXSIHZX,
   //P9EMU_LXSSP,
   //P9EMU_LXV,
   //P9EMU_LXVB16X,
   //P9EMU_LXVH8X,
   //P9EMU_LXVL,
   //P9EMU_LXVLL,
   //P9EMU_LXVWSX,
   //P9EMU_LXVX,
   //P9EMU_MADDHD,
   //P9EMU_MADDHDU,
   P9EMU_MADDLD,
   //P9EMU_MCRXRX,
   //P9EMU_MFVSRLD,
#ifdef TR_HOST_64BIT
   P9EMU_MODSD,
#endif
   P9EMU_MODSW,
#ifdef TR_HOST_64BIT
   P9EMU_MODUD,
#endif
   P9EMU_MODUW,
   //P9EMU_MSGSYNC,
   //P9EMU_MTVSRDD,
   //P9EMU_MTVSRWS,
   //P9EMU_PASTE,
   //P9EMU_PASTE_R,
   //P9EMU_RFSCV,
   //P9EMU_SCV,
   P9EMU_SETB,
   //P9EMU_SLBIEG,
   //P9EMU_SLBSYNC,
   //P9EMU_STDAT,
   //P9EMU_STOP,
   //P9EMU_STWAT,
   //P9EMU_STXSD,
   //P9EMU_STXSIBX,
   //P9EMU_STXSIHX,
   //P9EMU_STXSSP,
   //P9EMU_STXV,
   //P9EMU_STXVB16X,
   //P9EMU_STXVH8X,
   //P9EMU_STXVL,
   //P9EMU_STXVLL,
   //P9EMU_STXVX,
   //P9EMU_VABSDUB,
   //P9EMU_VABSDUH,
   //P9EMU_VABSDUW,
   //P9EMU_VBPERMD,
   //P9EMU_VCLZLSBB,
   //P9EMU_VCMPNEB,
   //P9EMU_VCMPNEB_R,
   //P9EMU_VCMPNEH,
   //P9EMU_VCMPNEH_R,
   //P9EMU_VCMPNEW,
   //P9EMU_VCMPNEW_R,
   //P9EMU_VCMPNEZB,
   //P9EMU_VCMPNEZB_R,
   //P9EMU_VCMPNEZH,
   //P9EMU_VCMPNEZH_R,
   //P9EMU_VCMPNEZW,
   //P9EMU_VCMPNEZW_R,
   //P9EMU_VCTZB,
   //P9EMU_VCTZD,
   //P9EMU_VCTZH,
   //P9EMU_VCTZLSBB,
   //P9EMU_VCTZW,
   //P9EMU_VEXTRACTD,
   //P9EMU_VEXTRACTUB,
   //P9EMU_VEXTRACTUH,
   //P9EMU_VEXTRACTUW,
   //P9EMU_VEXTSB2D,
   //P9EMU_VEXTSB2W,
   //P9EMU_VEXTSH2D,
   //P9EMU_VEXTSH2W,
   //P9EMU_VEXTSW2D,
   //P9EMU_VEXTUBLX,
   //P9EMU_VEXTUBRX,
   //P9EMU_VEXTUHLX,
   //P9EMU_VEXTUHRX,
   //P9EMU_VEXTUWLX,
   //P9EMU_VEXTUWRX,
   //P9EMU_VINSERTB,
   //P9EMU_VINSERTD,
   //P9EMU_VINSERTH,
   //P9EMU_VINSERTW,
   //P9EMU_VMUL10CUQ,
   //P9EMU_VMUL10ECUQ,
   //P9EMU_VMUL10EUQ,
   //P9EMU_VMUL10UQ,
   //P9EMU_VNEGD,
   //P9EMU_VNEGW,
   //P9EMU_VPERMR,
   //P9EMU_VPRTYBD,
   //P9EMU_VPRTYBQ,
   //P9EMU_VPRTYBW,
   //P9EMU_VRLDMI,
   //P9EMU_VRLDNM,
   //P9EMU_VRLWMI,
   //P9EMU_VRLWNM,
   //P9EMU_VSLV,
   //P9EMU_VSRV,
   //P9EMU_WAIT,
   //P9EMU_XSABSQP,
   //P9EMU_XSADDQP,
   //P9EMU_XSADDQPO,
   //P9EMU_XSCMPEQDP,
   //P9EMU_XSCMPEXPDP,
   //P9EMU_XSCMPEXPQP,
   //P9EMU_XSCMPGEDP,
   //P9EMU_XSCMPGTDP,
   //P9EMU_XSCMPNEDP,
   //P9EMU_XSCMPOQP,
   //P9EMU_XSCMPUQP,
   //P9EMU_XSCPSGNQP,
   //P9EMU_XSCVDPHP,
   //P9EMU_XSCVDPQP,
   //P9EMU_XSCVHPDP,
   //P9EMU_XSCVQPDP,
   //P9EMU_XSCVQPDPO,
   //P9EMU_XSCVQPSDZ,
   //P9EMU_XSCVQPSWZ,
   //P9EMU_XSCVQPUDZ,
   //P9EMU_XSCVQPUWZ,
   //P9EMU_XSCVSDQP,
   //P9EMU_XSCVUDQP,
   //P9EMU_XSDIVQP,
   //P9EMU_XSDIVQPO,
   //P9EMU_XSIEXPDP,
   //P9EMU_XSIEXPQP,
   //P9EMU_XSMADDQP,
   //P9EMU_XSMADDQPO,
   //P9EMU_XSMAXCDP,
   //P9EMU_XSMAXJDP,
   //P9EMU_XSMINCDP,
   //P9EMU_XSMINJDP,
   //P9EMU_XSMSUBQP,
   //P9EMU_XSMSUBQPO,
   //P9EMU_XSMULQP,
   //P9EMU_XSMULQPO,
   //P9EMU_XSNABSQP,
   //P9EMU_XSNEGQP,
   //P9EMU_XSNMADDQP,
   //P9EMU_XSNMADDQPO,
   //P9EMU_XSNMSUBQP,
   //P9EMU_XSNMSUBQPO,
   //P9EMU_XSRQPI,
   //P9EMU_XSRQPIX,
   //P9EMU_XSRQPXP,
   //P9EMU_XSSQRTQP,
   //P9EMU_XSSQRTQPO,
   //P9EMU_XSSUBQP,
   //P9EMU_XSSUBQPO,
   //P9EMU_XSTSTDCDP,
   //P9EMU_XSTSTDCQP,
   //P9EMU_XSTSTDCSP,
   //P9EMU_XSXEXPDP,
   //P9EMU_XSXEXPQP,
   //P9EMU_XSXSIGDP,
   //P9EMU_XSXSIGQP,
   //P9EMU_XVCMPNEDP,
   //P9EMU_XVCMPNEDP_R,
   //P9EMU_XVCMPNESP,
   //P9EMU_XVCMPNESP_R,
   //P9EMU_XVCVHPSP,
   //P9EMU_XVCVSPHP,
   //P9EMU_XVIEXPDP,
   //P9EMU_XVIEXPSP,
   //P9EMU_XVTSTDCDP,
   //P9EMU_XVTSTDCSP,
   //P9EMU_XVXEXPDP,
   //P9EMU_XVXEXPSP,
   //P9EMU_XVXSIGDP,
   //P9EMU_XVXSIGSP,
   //P9EMU_XXBRD,
   //P9EMU_XXBRH,
   //P9EMU_XXBRQ,
   //P9EMU_XXBRW,
   //P9EMU_XXEXTRACTUW,
   //P9EMU_XXINSERTW,
   //P9EMU_XXPERM,
   //P9EMU_XXPERMR,
   //P9EMU_XXSPLTIB,

   P9EMU_NUM_HANDLED_INSTRUCTIONS,
   P9EMU_UNHANDLED_INSTRUCTION,
   } p9emu_insn_id_t;

typedef struct p9emu_ctx
   {
#ifdef TR_HOST_64BIT
   uint64_t lmrr, lmser;
#endif
   uint64_t bescr;
   uintptr_t ebbhr, ebbrr;
   } p9emu_ctx_t;

static __thread p9emu_ctx_t p9emu_ctx = {0};

#define PRIMARY_OPCODE(insn)            (((insn) & 0xFC000000u) >> 26)

#define XFORM_EXTENDED_OPCODE(insn)     (((insn) & 0x000007FEu) >>  1)
#define XFORM_BF(insn)                  (((insn) & 0x03800000u) >> 23)
#define XFORM_L10(insn)                 (((insn) & 0x00200000u) >> 21)
#define XFORM_RT(insn)                  (((insn) & 0x03E00000u) >> 21)
#define XFORM_BFA(insn)                 (((insn) & 0x001C0000u) >> 18)
#define XFORM_RA(insn)                  (((insn) & 0x001F0000u) >> 16)
#define XFORM_RB(insn)                  (((insn) & 0x0000F800u) >> 11)

#define XSFORM_EXTENDED_OPCODE(insn)    (((insn) & 0x000007FCu) >>  2)
#define XSFORM_SH(insn)                 ((((insn) & 0x0000F800u) >> 11) | (((insn) & 0x00000002u) << 4))
#define XSFORM_RA(insn)                 XFORM_RA(insn)
#define XSFORM_RS(insn)                 XFORM_RT(insn)

#define DXFORM_EXTENDED_OPCODE(insn)    (((insn) & 0x0000003Eu) >> 1)
#define DXFORM_D(insn)                  ((int16_t)(((insn) & 0x0000FFC0u) | (((insn) & 0x001F0000u) >> 15) | ((insn) & 0x00000001u)))
#define DXFORM_RT(insn)                 XFORM_RT(insn)

#define XFXFORM_RT(insn)                XFORM_RT(insn)
#define XFXFORM_RS(insn)                XFXFORM_RT(insn)
#define XFXFORM_SPR(insn)               ((((insn) & 0x0000F800u) >> 6) | (((insn) & 0x001F0000u) >> 16))

#define XLFORM_EXTENDED_OPCODE(insn)    XFORM_EXTENDED_OPCODE(insn)
#define XLFORM_S(insn)                  (((insn) & 0x00000800u) >> 11)
#define VAFORM_EXTENDED_OPCODE(insn)    (insn & 0x3f)
#define VAFORM_RA(insn)                 XFORM_RA(insn)
#define VAFORM_RB(insn)                 XFORM_RB(insn)
#define VAFORM_RC(insn)                 (((insn) & 0x000007c0) >> 6)
#define VAFORM_RT(insn)                 XFORM_RT(insn)


static
void setrc(intptr_t r, unsigned long *gpregs)
   {
   uint32_t cr = gpregs[PT_CCR];
   uint32_t xer = gpregs[PT_XER];

   uint32_t lt = r < 0;
   uint32_t gt = r > 0;
   uint32_t eq = r == 0;
   uint32_t so = xer >> 31;
   uint32_t cr0 = lt << 3 | gt << 2 | eq << 1 | so;

   gpregs[PT_CCR] = (cr & 0x0FFFFFFFu) | (cr0 << 28);
   }

static
void cmprb(uint32_t insn, unsigned long *gpregs)
   {
   uintptr_t ra = gpregs[XFORM_RA(insn)];
   uintptr_t rb = gpregs[XFORM_RB(insn)];

   uint8_t src1         = (ra >>  0) & 0xFFu;
   uint8_t src21hi      = (rb >> 24) & 0xFFu;
   uint8_t src21lo      = (rb >> 16) & 0xFFu;
   uint8_t src22hi      = (rb >>  8) & 0xFFu;
   uint8_t src22lo      = (rb >>  0) & 0xFFu;

   uint32_t in_range;
   if (XFORM_L10(insn) == 0)
      in_range = (src22lo <= src1) && (src1 <= src22hi);
   else
      in_range = (src21lo <= src1) && (src1 <= src21hi) ||
                 (src22lo <= src1) && (src1 <= src22hi);

   uint32_t cr = gpregs[PT_CCR];
   uint32_t bf = XFORM_BF(insn);
   uint32_t mask = ~(0xF0000000u >> (bf * 4));

   gpregs[PT_CCR] = (cr & mask) | (in_range << 2 << (7 - bf) * 4);
   }

#ifdef TR_HOST_64BIT
static
void cmpeqb(uint32_t insn, unsigned long *gpregs)
   {
   uintptr_t ra = gpregs[XFORM_RA(insn)];
   uintptr_t rb = gpregs[XFORM_RB(insn)];

   uint8_t src1 = ra & 0xFFu;
   uint32_t match = src1 == ((rb >>  0) & 0xFFu) ||
                    src1 == ((rb >>  8) & 0xFFu) ||
                    src1 == ((rb >> 16) & 0xFFu) ||
                    src1 == ((rb >> 24) & 0xFFu) ||
                    src1 == ((rb >> 32) & 0xFFu) ||
                    src1 == ((rb >> 40) & 0xFFu) ||
                    src1 == ((rb >> 48) & 0xFFu) ||
                    src1 == ((rb >> 56) & 0xFFu);

   uint32_t cr = gpregs[PT_CCR];
   uint32_t bf = XFORM_BF(insn);
   uint32_t mask = ~(0xF0000000u >> (bf * 4));

   gpregs[PT_CCR] = (cr & mask) | (match << 2 << (7 - bf) * 4);
   }
#endif

static
void modsw(uint32_t insn, unsigned long *gpregs)
   {
   uintptr_t ra = gpregs[XFORM_RA(insn)];
   uintptr_t rb = gpregs[XFORM_RB(insn)];

   int32_t dividend = (int32_t)ra;
   int32_t divisor = (int32_t)rb;
   int32_t remainder = dividend % divisor;

   gpregs[XFORM_RT(insn)] = (intptr_t)remainder;
   }

static
void moduw(uint32_t insn, unsigned long *gpregs)
   {
   uintptr_t ra = gpregs[XFORM_RA(insn)];
   uintptr_t rb = gpregs[XFORM_RB(insn)];

   uint32_t dividend = (uint32_t)ra;
   uint32_t divisor = (uint32_t)rb;
   uint32_t remainder = dividend % divisor;

   gpregs[XFORM_RT(insn)] = (uintptr_t)remainder;
   }

#ifdef TR_HOST_64BIT
static
void modsd(uint32_t insn, unsigned long *gpregs)
   {
   uintptr_t ra = gpregs[XFORM_RA(insn)];
   uintptr_t rb = gpregs[XFORM_RB(insn)];

   int64_t dividend = (int64_t)ra;
   int64_t divisor = (int64_t)rb;
   int64_t remainder = dividend % divisor;

   gpregs[XFORM_RT(insn)] = (intptr_t)remainder;
   }

static
void modud(uint32_t insn, unsigned long *gpregs)
   {
   uintptr_t ra = gpregs[XFORM_RA(insn)];
   uintptr_t rb = gpregs[XFORM_RB(insn)];

   uint64_t dividend = (uint64_t)ra;
   uint64_t divisor = (uint64_t)rb;
   uint64_t remainder = dividend % divisor;

   gpregs[XFORM_RT(insn)] = (uintptr_t)remainder;
   }
#endif

static
void setb(uint32_t insn, unsigned long *gpregs)
   {
   uint32_t  bfa = XFORM_BFA(insn);
   uintptr_t ccr = gpregs[PT_CCR];
   uintptr_t crf = ccr >> (7 - bfa) * 4;
   intptr_t  rt;

   if (crf & 8)
      rt = -1;
   else if (crf & 4)
      rt = 1;
   else
      rt = 0;

   gpregs[XFORM_RT(insn)] = rt;
   }

static
void extswsli(uint32_t insn, unsigned long *gpregs)
   {
   uint32_t sh = XSFORM_SH(insn);
   intptr_t rs = gpregs[XSFORM_RS(insn)];
   intptr_t ra = ((intptr_t)(int32_t)rs) << sh;

   gpregs[XSFORM_RA(insn)] = ra;

   if (insn & 1)
      setrc(ra, gpregs);
   }

static
void addpcis(uint32_t insn, unsigned long *gpregs)
   {
   intptr_t  d = DXFORM_D(insn);
   uintptr_t nia = gpregs[PT_NIP] + 4;

   gpregs[DXFORM_RT(insn)] = nia + (d << 16);
   }

static
void maddld(uint32_t insn, unsigned long *gpregs)
   {
   uintptr_t ra = gpregs[VAFORM_RA(insn)];
   uintptr_t rb = gpregs[VAFORM_RB(insn)];
   uintptr_t rc = gpregs[VAFORM_RC(insn)];
   uint64_t op1 = (uint64_t) ra;
   uint64_t op2 = (uint64_t) rb;
   uint64_t op3 = (uint64_t) rc;
   uint64_t rt =  ((op1 * op2) + op3);

   gpregs[VAFORM_RT(insn)] = (uintptr_t) rt;
   }

static
void mtspr(uint32_t insn, unsigned long *gpregs, p9emu_ctx_t *ctx)
   {
   uint32_t  spr = XFXFORM_SPR(insn);
   uintptr_t rs = gpregs[XFXFORM_RS(insn)];

   switch (spr)
      {
      case 800:
         ctx->bescr |= (uint64_t)rs & 0x80000007C0000007u;
         break;
      case 801:
         ctx->bescr |= (uint64_t)rs << 32 & 0x80000007C0000007u;
         break;
      case 802:
         ctx->bescr &= ~(uint64_t)rs;
         break;
      case 803:
         ctx->bescr &= ~((uint64_t)rs << 32);
         break;
      case 804:
         ctx->ebbhr = rs;
         break;
      case 805:
         ctx->ebbrr = rs & ~3;
         break;
#ifdef TR_HOST_64BIT
      case 806:
         ctx->bescr = (uint64_t)rs & 0x80000007C0000007u;
         break;
      case 813:
         ctx->lmrr = rs;
         break;
      case 814:
         ctx->lmser = rs;
         break;
#endif
      }
   }

static
void mfspr(uint32_t insn, unsigned long *gpregs, p9emu_ctx_t *ctx)
   {
   uint32_t  spr = XFXFORM_SPR(insn);
   uintptr_t rt;

   switch (spr)
      {
      case 800:
      case 801:
      case 802:
      case 803:
         rt = 0;
         break;
      case 804:
         rt = ctx->ebbhr;
         break;
      case 805:
         rt = ctx->ebbrr;
         break;
#ifdef TR_HOST_64BIT
      case 806:
         rt = ctx->bescr;
         break;
      case 813:
         rt = ctx->lmrr;
         break;
      case 814:
         rt = ctx->lmser;
         break;
#endif
      }

   gpregs[XFXFORM_RT(insn)] = rt;
   }

static
void rfebb(uint32_t insn, unsigned long *gpregs, p9emu_ctx_t *ctx)
   {
   uint32_t s = XLFORM_S(insn);

   ctx->bescr = (ctx->bescr & ~0x8000000000000000u) | (uint64_t)s << 63;

   gpregs[PT_NIP] = ctx->ebbrr;
   }

#ifdef TR_HOST_64BIT
static
void ldmx(uint32_t insn, unsigned long *gpregs, p9emu_ctx_t *ctx)
   {
   uintptr_t ra = XFORM_RA(insn) == 0 ? 0 : gpregs[XFORM_RA(insn)];
   uintptr_t rb = gpregs[XFORM_RB(insn)];
   uintptr_t ea = ra + rb;
   uint64_t  loaded_ea = *(uint64_t *)ea;

   if ((ctx->bescr >> 32 & 0x80000004u) == 0x80000004u)
      {
      uint64_t  lmrr = ctx->lmrr;
      uint64_t  lmr_size = lmrr & 0xF;
      uint64_t  size = (uint64_t)1 << (lmr_size + 25);
      uint64_t  base_mask = ~(size - 1);
      uint64_t  base = lmrr & base_mask;

      uint64_t  region_size = size / 64;
      uint64_t  region_size_log2 = trailingZeroes(region_size);
      uint64_t  loaded_ea_region = (loaded_ea - base) >> region_size_log2;
      uint64_t  loaded_ea_region_bit = (uint64_t)1 << (63 - loaded_ea_region);

      if (loaded_ea_region_bit & ctx->lmser)
         {
         ctx->bescr = (ctx->bescr & ~0x8000000400000000u) | 0x0000000000000004u;
         ctx->ebbrr = gpregs[PT_NIP] + 4;
         gpregs[PT_NIP] = ctx->ebbhr;

         return;
         }
      }

   gpregs[XFORM_RT(insn)] = loaded_ea;
   gpregs[PT_NIP] = gpregs[PT_NIP] + 4;
   }
#endif

static
p9emu_insn_id_t p9emu_decode_insn_p4(uint32_t insn)
   {
   switch (VAFORM_EXTENDED_OPCODE(insn))
      {
      case 51:
         return P9EMU_MADDLD;
      }
   return P9EMU_UNHANDLED_INSTRUCTION;
   }


static
p9emu_insn_id_t p9emu_decode_insn_p19(uint32_t insn)
   {
   switch (XLFORM_EXTENDED_OPCODE(insn))
      {
      case 146:
         return P9EMU_RFEBB;
      }
   switch (DXFORM_EXTENDED_OPCODE(insn))
      {
      case 2:
         return P9EMU_ADDPCIS;
      }
   return P9EMU_UNHANDLED_INSTRUCTION;
   }

static
p9emu_insn_id_t p9emu_decode_insn_p31(uint32_t insn)
   {
   uint32_t ext = XFORM_EXTENDED_OPCODE(insn);
   switch (ext)
      {
      case 128:
         return P9EMU_SETB;
      case 192:
         return P9EMU_CMPRB;
#ifdef TR_HOST_64BIT
      case 224:
         return P9EMU_CMPEQB;
#endif
#ifdef TR_HOST_64BIT
      case 265:
         return P9EMU_MODUD;
#endif
      case 267:
         return P9EMU_MODUW;
#ifdef TR_HOST_64BIT
      case 309:
         return P9EMU_LDMX;
#endif
      case 339:
      case 467:
         switch (XFXFORM_SPR(insn))
            {
            case 800:
            case 801:
            case 802:
            case 803:
            case 804:
            case 805:
#ifdef TR_HOST_64BIT
            case 806:
            case 813:
            case 814:
#endif
               return ext == 339 ? P9EMU_MFSPR : P9EMU_MTSPR;
            }
         break;
#ifdef TR_HOST_64BIT
      case 777:
         return P9EMU_MODSD;
#endif
      case 779:
         return P9EMU_MODSW;
      }
   switch (XSFORM_EXTENDED_OPCODE(insn))
      {
      case 445:
         return insn & 1 ? P9EMU_EXTSWSLI_R : P9EMU_EXTSWSLI;
      }
   return P9EMU_UNHANDLED_INSTRUCTION;
   }

static
p9emu_insn_id_t p9emu_decode_insn(uint32_t insn)
   {
   switch (PRIMARY_OPCODE(insn))
      {
      case 4:
         return p9emu_decode_insn_p4(insn);
      case 19:
         return p9emu_decode_insn_p19(insn);
      case 31:
         return p9emu_decode_insn_p31(insn);
      default:
         return P9EMU_UNHANDLED_INSTRUCTION;
      }
   }

static
int32_t p9emu_process(p9emu_ctx_t *ctx, ucontext_t *uctx)
   {
#ifdef TR_HOST_64BIT
   unsigned long *gpregs = uctx->uc_mcontext.gp_regs;
#else
   unsigned long *gpregs = uctx->uc_mcontext.uc_regs->gregs;
#endif
   unsigned long iar = gpregs[PT_NIP];

   if (!iar)
      return -1;

   uint32_t insn = *(uint32_t *)iar;

   p9emu_insn_id_t insn_id = p9emu_decode_insn(insn);
   if (insn_id == P9EMU_UNHANDLED_INSTRUCTION)
      return -1;

   switch (insn_id)
      {
      case P9EMU_CMPRB:
         cmprb(insn, gpregs);
         break;
#ifdef TR_HOST_64BIT
      case P9EMU_CMPEQB:
         cmpeqb(insn, gpregs);
         break;
#endif
#ifdef TR_HOST_64BIT
      case P9EMU_LDMX:
         ldmx(insn, gpregs, ctx);
         /* Return here as ldmx modifies the instruction pointer, no need to increment it. */
         return 0;
#endif
#ifdef TR_HOST_64BIT
      case P9EMU_MODSD:
         modsd(insn, gpregs);
         break;
#endif
      case P9EMU_MODSW:
         modsw(insn, gpregs);
         break;
#ifdef TR_HOST_64BIT
      case P9EMU_MODUD:
         modud(insn, gpregs);
         break;
#endif
      case P9EMU_MODUW:
         moduw(insn, gpregs);
         break;
      case P9EMU_MFSPR:
         mfspr(insn, gpregs, ctx);
         break;
      case P9EMU_MTSPR:
         mtspr(insn, gpregs, ctx);
         break;
      case P9EMU_RFEBB:
         rfebb(insn, gpregs, ctx);
         /* Return here as rfebb modifies the instruction pointer, no need to increment it. */
         return 0;
      case P9EMU_SETB:
         setb(insn, gpregs);
         break;
      case P9EMU_EXTSWSLI:
      case P9EMU_EXTSWSLI_R:
         extswsli(insn, gpregs);
         break;
      case P9EMU_ADDPCIS:
         addpcis(insn, gpregs);
         break;
      case P9EMU_MADDLD:
         maddld(insn, gpregs);
         break;
      }

   gpregs[PT_NIP] = iar + 4;

   return 0;
   }

#endif /* LINUX */

UDATA jitPPCEmulation(J9VMThread *vmThread, void *sigInfo)
   {
#ifdef LINUX
   OMRUnixSignalInfo *unixSigInfo = (OMRUnixSignalInfo *)sigInfo;
   if (p9emu_process(&p9emu_ctx, unixSigInfo->platformSignalInfo.context) != 0)
      {
      return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
      }
   return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;
#else
   return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
#endif
   }

