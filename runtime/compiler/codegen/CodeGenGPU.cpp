/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

/**
 * Support code for TR::CodeGenerator.  Code related to generating GPU
 */

#if defined (_MSC_VER) && (_MSC_VER < 1900)
#define snprintf _snprintf
#endif

#include <stdio.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "env/CompilerEnv.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/annotations/GPUAnnotation.hpp"
#include "optimizer/Dominators.hpp"
#include "optimizer/Structure.hpp"

#define OPT_DETAILS "O^O CODE GENERATION: "


static const char * nvvmOpCodeNames[] =
   {
   NULL,          // TR::BadILOp
   NULL,          // TR::aconst
   NULL,          // TR::iconst
   NULL,          // TR::lconst
   NULL,          // TR::fconst
   NULL,          // TR::dconst
   NULL,          // TR::bconst
   NULL,          // TR::sconst

   "load",          // TR::iload
   "load",          // TR::fload
   "load",          // TR::dload
   "load",          // TR::aload
   "load",          // TR::bload
   "load",          // TR::sload
   "load",          // TR::lload
   NULL,            // TR::irdbar
   NULL,            // TR::frdbar
   NULL,            // TR::drdbar
   NULL,            // TR::ardbar
   NULL,            // TR::brdbar
   NULL,            // TR::srdbar
   NULL,            // TR::lrdbar
   "load",          // TR::iloadi
   "load",          // TR::floadi
   "load",          // TR::dloadi
   "load",          // TR::aloadi
   "load",          // TR::bloadi
   "load",          // TR::sloadi
   "load",          // TR::lloadi
   NULL,            // TR::irdbari
   NULL,            // TR::frdbari
   NULL,            // TR::drdbari
   NULL,            // TR::ardbari
   NULL,            // TR::brdbari
   NULL,            // TR::srdbari
   NULL,            // TR::lrdbari
   "store",          // TR::istore
   "store",          // TR::lstore
   "store",          // TR::fstore
   "store",          // TR::dstore
   "store",          // TR::astore
   "store",          // TR::bstore
   "store",          // TR::sstore
   NULL,             // TR::iwrtbar
   NULL,             // TR::lwrtbar
   NULL,             // TR::fwrtbar
   NULL,             // TR::dwrtbar
   NULL,             // TR::awrtbar
   NULL,             // TR::bwrtbar
   NULL,             // TR::swrtbar
   "store",          // TR::lstorei
   "store",          // TR::fstorei
   "store",          // TR::dstorei
   "store",          // TR::astorei
   "store",          // TR::bstorei
   "store",          // TR::sstorei
   "store",          // TR::istorei
   NULL,             // TR::lwrtbari
   NULL,             // TR::fwrtbari
   NULL,             // TR::dwrtbari
   NULL,             // TR::awrtbari
   NULL,             // TR::bwrtbari
   NULL,             // TR::swrtbari
   NULL,             // TR::iwrtbari
   "br",          // TR::Goto
   "ret",          // TR::ireturn
   "ret",          // TR::lreturn
   "ret",          // TR::freturn
   "ret",          // TR::dreturn
   "ret",          // TR::areturn
   "ret",          // TR::Return
   NULL,          // TR::asynccheck
   NULL,          // TR::athrow
   NULL,          // TR::icall
   NULL,          // TR::lcall
   NULL,          // TR::fcall
   NULL,          // TR::dcall
   NULL,          // TR::acall
   NULL,          // TR::call
   "add",         // TR::iadd
   "add",         // TR::ladd
   "fadd",        // TR::fadd
   "fadd",        // TR::dadd
   "add",         // TR::badd
   "add",         // TR::sadd

   "sub",         // TR::isub
   "sub",         // TR::lsub
   "fsub",        // TR::fsub
   "fsub",        // TR::dsub
   "sub",         // TR::bsub
   "sub",         // TR::ssub
   NULL,          // TR::asub

   "mul",         // TR::imul
   "mul",         // TR::lmul
   "fmul",        // TR::fmul
   "fmul",        // TR::dmul
   "mul",         // TR::bmul
   "mul",         // TR::smul
   "mul",         // TR::iumul

   "sdiv",        // TR::idiv
   "sdiv",        // TR::ldiv
   "fdiv",        // TR::fdiv
   "fdiv",        // TR::ddiv
   "sdiv",        // TR::bdiv
   "sdiv",        // TR::sdiv
   "udiv",        // TR::iudiv
   "udiv",        // TR::ludiv

   "srem",          // TR::irem
   "srem",          // TR::lrem
   "frem",          // TR::frem
   "frem",          // TR::drem
   "srem",          // TR::brem
   "srem",          // TR::srem
   "urem",          // TR::iurem
   "sub",           // TR::ineg
   "sub",           // TR::lneg
   "fsub",          // TR::fneg
   "fsub",          // TR::dneg
   "sub",           // TR::bneg
   "sub",           // TR::sneg

   NULL,          // TR::iabs, implemented but this table value is unused
   NULL,          // TR::labs, implemented but this table value is unused
   NULL,          // TR::fabs
   NULL,          // TR::dabs

   "shl",         // TR::ishl
   "shl",         // TR::lshl
   "shl",         // TR::bshl
   "shl",         // TR::sshl
   "ashr",        // TR::ishr
   "ashr",        // TR::lshr
   "ashr",        // TR::bshr
   "ashr",        // TR::sshr
   "lshr",        // TR::iushr
   "lshr",        // TR::lushr
   "lshr",        // TR::bushr
   "lshr",        // TR::sushr
   NULL,          // TR::irol, implemented but this table value is unused
   NULL,          // TR::lrol, implemented but this table value is unused

   "and",         // TR::iand
   "and",         // TR::land
   "and",         // TR::band
   "and",         // TR::sand
   "or",          // TR::ior
   "or",          // TR::lor
   "or",          // TR::bor
   "or",          // TR::sor
   "xor",         // TR::ixor
   "xor",         // TR::lxor
   "xor",         // TR::bxor
   "xor",         // TR::sxor
   "sext",        // TR::i2l
   "sitofp",      // TR::i2f
   "sitofp",      // TR::i2d
   "trunc",       // TR::i2b
   "trunc",       // TR::i2s
   "inttoptr",    // TR::i2a
   "zext",        // TR::iu2l
   "uitofp",      // TR::iu2f
   "uitofp",      // TR::iu2d
   "inttoptr",    // TR::iu2a
   "trunc",       // TR::l2i
   "sitofp",      // TR::l2f
   "sitofp",      // TR::l2d
   "trunc",       // TR::l2b
   "trunc",       // TR::l2s
   "inttoptr",    // TR::l2a
   "uitofp",      // TR::lu2f
   "uitofp",      // TR::lu2d
   "inttoptr",    // TR::lu2a
   "fptosi",      // TR::f2i
   "fptosi",      // TR::f2l
   "fpext",       // TR::f2d
   "fptosi",      // TR::f2b
   "fptosi",      // TR::f2s
   "fptosi",      // TR::d2i
   "fptosi",      // TR::d2l
   "fptrunc",     // TR::d2f
   "fptosi",      // TR::d2b
   "fptosi",      // TR::d2s
   "sext",        // TR::b2i
   "sext",        // TR::b2l
   "sitofp",      // TR::b2f
   "sitofp",      // TR::b2d
   "sext",        // TR::b2s
   "inttoptr",    // TR::b2a
   "zext",        // TR::bu2i
   "zext",        // TR::bu2l
   "uitofp",      // TR::bu2f
   "uitofp",      // TR::bu2d
   "zext",        // TR::bu2s
   "inttoptr",    // TR::bu2a
   "sext",        // TR::s2i
   "sext",        // TR::s2l
   "sitofp",      // TR::s2f
   "sitofp",      // TR::s2d
   "trunc",       // TR::s2b
   "inttoptr",    // TR::s2a
   "zext",        // TR::su2i
   "zext",        // TR::su2l
   "uitofp",      // TR::su2f
   "uitofp",      // TR::su2d
   "inttoptr",    // TR::su2a
   "ptrtoint",    // TR::a2i
   "ptrtoint",    // TR::a2l
   "ptrtoint",    // TR::a2b
   "ptrtoint",    // TR::a2s
   "icmp eq",     // TR::icmpeq
   "icmp ne",     // TR::icmpne
   "icmp slt",    // TR::icmplt
   "icmp sge",    // TR::icmpge
   "icmp sgt",    // TR::icmpgt
   "icmp sle",    // TR::icmple
   "icmp eq",     // TR::iucmpeq
   "icmp ne",     // TR::iucmpne
   "icmp ult",    // TR::iucmplt
   "icmp uge",    // TR::iucmpge
   "icmp ugt",    // TR::iucmpgt
   "icmp ule",    // TR::iucmple
   "icmp eq",     // TR::lcmpeq
   "icmp ne",     // TR::lcmpne
   "icmp slt",    // TR::lcmplt
   "icmp sge",    // TR::lcmpge
   "icmp sgt",    // TR::lcmpgt
   "icmp sle",    // TR::lcmple
   "icmp eq",     // TR::lucmpeq
   "icmp ne",     // TR::lucmpne
   "icmp ult",    // TR::lucmplt
   "icmp uge",    // TR::lucmpge
   "icmp ugt",    // TR::lucmpgt
   "icmp ule",    // TR::lucmple
   "fcmp oeq",    // TR::fcmpeq
   "fcmp one",    // TR::fcmpne
   "fcmp olt",    // TR::fcmplt
   "fcmp oge",    // TR::fcmpge
   "fcmp ogt",    // TR::fcmpgt
   "fcmp ole",    // TR::fcmple
   "fcmp ueq",    // TR::fcmpequ
   "fcmp une",    // TR::fcmpneu
   "fcmp ult",    // TR::fcmpltu
   "fcmp uge",    // TR::fcmpgeu
   "fcmp ugt",    // TR::fcmpgtu
   "fcmp ule",    // TR::fcmpleu
   "fcmp oeq",    // TR::dcmpeq
   "fcmp one",    // TR::dcmpne
   "fcmp olt",    // TR::dcmplt
   "fcmp oge",    // TR::dcmpge
   "fcmp ogt",    // TR::dcmpgt
   "fcmp ole",    // TR::dcmple
   "fcmp ueq",    // TR::dcmpequ
   "fcmp une",    // TR::dcmpneu
   "fcmp ult",    // TR::dcmpltu
   "fcmp uge",    // TR::dcmpgeu
   "fcmp ugt",    // TR::dcmpgtu
   "fcmp ule",    // TR::dcmpleu
   "icmp eq",     // TR::acmpeq
   "icmp ne",     // TR::acmpne
   "icmp ult",    // TR::acmplt
   "icmp uge",    // TR::acmpge
   "icmp ugt",    // TR::acmpgt
   "icmp ule",    // TR::acmple
   "icmp eq",     // TR::bcmpeq
   "icmp ne",     // TR::bcmpne
   "icmp slt",    // TR::bcmplt
   "icmp sge",    // TR::bcmpge
   "icmp sgt",    // TR::bcmpgt
   "icmp sle",    // TR::bcmple
   "icmp eq",     // TR::bucmpeq
   "icmp ne",     // TR::bucmpne
   "icmp ult",    // TR::bucmplt
   "icmp uge",    // TR::bucmpge
   "icmp ugt",    // TR::bucmpgt
   "icmp ule",    // TR::bucmple
   "icmp eq",     // TR::scmpeq
   "icmp ne",     // TR::scmpne
   "icmp slt",    // TR::scmplt
   "icmp sge",    // TR::scmpge
   "icmp sgt",    // TR::scmpgt
   "icmp sle",    // TR::scmple
   "icmp eq",     // TR::sucmpeq
   "icmp ne",     // TR::sucmpne
   "icmp ult",    // TR::sucmplt
   "icmp uge",    // TR::sucmpge
   "icmp ugt",    // TR::sucmpgt
   "icmp ule",    // TR::sucmple
   NULL,          // TR::lcmp
   NULL,          // TR::fcmpl
   NULL,          // TR::fcmpg
   NULL,          // TR::dcmpl
   NULL,          // TR::dcmpg
   "icmp eq",     // TR::ificmpeq
   "icmp ne",     // TR::ificmpne
   "icmp slt",    // TR::ificmplt
   "icmp sge",    // TR::ificmpge
   "icmp sgt",    // TR::ificmpgt
   "icmp sle",    // TR::ificmple

   "icmp eq",     // TR::ifiucmpeq
   "icmp ne",     // TR::ifiucmpne
   "icmp ult",    // TR::ifiucmplt
   "icmp uge",    // TR::ifiucmpge
   "icmp ugt",    // TR::ifiucmpgt
   "icmp ule",    // TR::ifiucmple

   "icmp eq",     // TR::iflcmpeq
   "icmp ne",     // TR::iflcmpne
   "icmp slt",    // TR::iflcmplt
   "icmp sge",    // TR::iflcmpge
   "icmp sgt",    // TR::iflcmpgt
   "icmp sle",    // TR::iflcmple
   "icmp eq",     // TR::iflucmpeq
   "icmp ne",     // TR::iflucmpne
   "icmp ult",    // TR::iflucmplt
   "icmp uge",    // TR::iflucmpge
   "icmp ugt",    // TR::iflucmpgt
   "icmp ule",    // TR::iflucmple
   "fcmp oeq",    // TR::iffcmpeq
   "fcmp one",    // TR::iffcmpne
   "fcmp olt",    // TR::iffcmplt
   "fcmp oge",    // TR::iffcmpge
   "fcmp ogt",    // TR::iffcmpgt
   "fcmp ole",    // TR::iffcmple
   "fcmp ueq",    // TR::iffcmpequ
   "fcmp une",    // TR::iffcmpneu
   "fcmp ult",    // TR::iffcmpltu
   "fcmp uge",    // TR::iffcmpgeu
   "fcmp ugt",    // TR::iffcmpgtu
   "fcmp ule",    // TR::iffcmpleu
   "fcmp oeq",    // TR::ifdcmpeq
   "fcmp one",    // TR::ifdcmpne
   "fcmp olt",    // TR::ifdcmplt
   "fcmp oge",    // TR::ifdcmpge
   "fcmp ogt",    // TR::ifdcmpgt
   "fcmp ole",    // TR::ifdcmple
   "fcmp ueq",    // TR::ifdcmpequ
   "fcmp une",    // TR::ifdcmpneu
   "fcmp ult",    // TR::ifdcmpltu
   "fcmp uge",    // TR::ifdcmpgeu
   "fcmp ugt",    // TR::ifdcmpgtu
   "fcmp ule",    // TR::ifdcmpleu

   "icmp eq",     // TR::ifacmpeq
   "icmp ne",     // TR::ifacmpne
   "icmp ult",    // TR::ifacmplt
   "icmp uge",    // TR::ifacmpge
   "icmp ugt",    // TR::ifacmpgt
   "icmp ule",    // TR::ifacmple

   "icmp eq",     // TR::ifbcmpeq
   "icmp ne",     // TR::ifbcmpne
   "icmp slt",    // TR::ifbcmplt
   "icmp sge",    // TR::ifbcmpge
   "icmp sgt",    // TR::ifbcmpgt
   "icmp sle",    // TR::ifbcmple
   "icmp eq",     // TR::ifbucmpeq
   "icmp ne",     // TR::ifbucmpne
   "icmp ult",    // TR::ifbucmplt
   "icmp uge",    // TR::ifbucmpge
   "icmp ugt",    // TR::ifbucmpgt
   "icmp ule",    // TR::ifbucmple
   "icmp eq",     // TR::ifscmpeq
   "icmp ne",     // TR::ifscmpne
   "icmp slt",    // TR::ifscmplt
   "icmp sge",    // TR::ifscmpge
   "icmp sgt",    // TR::ifscmpgt
   "icmp sle",    // TR::ifscmple
   "icmp eq",     // TR::ifsucmpeq
   "icmp ne",     // TR::ifsucmpne
   "icmp ult",    // TR::ifsucmplt
   "icmp uge",    // TR::ifsucmpge
   "icmp ugt",    // TR::ifsucmpgt
   "icmp ule",    // TR::ifsucmple
   NULL,          // TR::loadaddr
   NULL,          // TR::ZEROCHK
   NULL,          // TR::callIf
   NULL,          // TR::iRegLoad
   NULL,          // TR::aRegLoad
   NULL,          // TR::lRegLoad
   NULL,          // TR::fRegLoad
   NULL,          // TR::dRegLoad
   NULL,          // TR::sRegLoad
   NULL,          // TR::bRegLoad
   NULL,          // TR::iRegStore
   NULL,          // TR::aRegStore
   NULL,          // TR::lRegStore
   NULL,          // TR::fRegStore
   NULL,          // TR::dRegStore
   NULL,          // TR::sRegStore
   NULL,          // TR::bRegStore
   NULL,          // TR::GlRegDeps

   NULL,          // TR::iternary
   NULL,          // TR::lternary
   NULL,          // TR::bternary
   NULL,          // TR::sternary
   NULL,          // TR::aternary
   NULL,          // TR::fternary
   NULL,          // TR::dternary
   NULL,          // TR::treetop
   NULL,          // TR::MethodEnterHook
   NULL,          // TR::MethodExitHook
   NULL,          // TR::PassThrough
   NULL,          // TR::compressedRefs

   "",          // TR::BBStart
   "",           // TR::BBEnd

   NULL,          // TR::virem
   NULL,          // TR::vimin
   NULL,          // TR::vimax
   NULL,          // TR::vigetelem
   NULL,          // TR::visetelem
   NULL,          // TR::vimergel
   NULL,          // TR::vimergeh

   NULL,          // TR::vicmpeq
   NULL,          // TR::vicmpgt
   NULL,          // TR::vicmpge
   NULL,          // TR::vicmplt
   NULL,          // TR::vicmple

   NULL,          // TR::vicmpalleq
   NULL,          // TR::vicmpallne
   NULL,          // TR::vicmpallgt
   NULL,          // TR::vicmpallge
   NULL,          // TR::vicmpalllt
   NULL,          // TR::vicmpallle
   NULL,          // TR::vicmpanyeq
   NULL,          // TR::vicmpanyne
   NULL,          // TR::vicmpanygt
   NULL,          // TR::vicmpanyge
   NULL,          // TR::vicmpanylt
   NULL,          // TR::vicmpanyle

   NULL,          // TR::vnot
   NULL,          // TR::vselect
   NULL,          // TR::vperm

   NULL,          // TR::vsplats
   NULL,          // TR::vdmergel
   NULL,          // TR::vdmergeh
   NULL,          // TR::vdsetelem
   NULL,          // TR::vdgetelem
   NULL,          // TR::vdsel

   NULL,          // TR::vdrem
   NULL,          // TR::vdmadd
   NULL,          // TR::vdnmsub
   NULL,          // TR::vdmsub
   NULL,          // TR::vdmax
   NULL,          // TR::vdmin

   NULL,          // TR::vdcmpeq
   NULL,          // TR::vdcmpne
   NULL,          // TR::vdcmpgt
   NULL,          // TR::vdcmpge
   NULL,          // TR::vdcmplt
   NULL,          // TR::vdcmple

   NULL,          // TR::vdcmpalleq
   NULL,          // TR::vdcmpallne
   NULL,          // TR::vdcmpallgt
   NULL,          // TR::vdcmpallge
   NULL,          // TR::vdcmpalllt
   NULL,          // TR::vdcmpallle

   NULL,          // TR::vdcmpanyeq
   NULL,          // TR::vdcmpanyne
   NULL,          // TR::vdcmpanygt
   NULL,          // TR::vdcmpanyge
   NULL,          // TR::vdcmpanylt
   NULL,          // TR::vdcmpanyle
   NULL,          // TR::vdsqrt
   NULL,          // TR::vdlog

   NULL,          // TR::vinc
   NULL,          // TR::vdec
   NULL,          // TR::vneg
   NULL,          // TR::vcom
   NULL,          // TR::vadd
   NULL,          // TR::vsub
   NULL,          // TR::vmul
   NULL,          // TR::vdiv
   NULL,          // TR::vrem
   NULL,          // TR::vand
   NULL,          // TR::vor
   NULL,          // TR::vxor
   NULL,          // TR::vshl
   NULL,          // TR::vushr
   NULL,          // TR::vshr
   NULL,          // TR::vcmpeq
   NULL,          // TR::vcmpne
   NULL,          // TR::vcmplt
   NULL,          // TR::vucmplt
   NULL,          // TR::vcmpgt
   NULL,          // TR::vucmpgt
   NULL,          // TR::vcmple
   NULL,          // TR::vucmple
   NULL,          // TR::vcmpge
   NULL,          // TR::vucmpge
   NULL,          // TR::vload
   NULL,          // TR::vloadi
   NULL,          // TR::vstore
   NULL,          // TR::vstorei
   NULL,          // TR::vrand
   NULL,          // TR::vreturn
   NULL,          // TR::vcall
   NULL,          // TR::vcalli
   NULL,          // TR::vternary
   NULL,          // TR::v2v
   NULL,          // TR::vl2vd
   NULL,          // TR::vconst
   NULL,          // TR::getvelem
   NULL,          // TR::vsetelem

   NULL,          // TR::vbRegLoad
   NULL,          // TR::vsRegLoad
   NULL,          // TR::viRegLoad
   NULL,          // TR::vlRegLoad
   NULL,          // TR::vfRegLoad
   NULL,          // TR::vdRegLoad
   NULL,          // TR::vbRegStore
   NULL,          // TR::vsRegStore
   NULL,          // TR::viRegStore
   NULL,          // TR::vlRegStore
   NULL,          // TR::vfRegStore
   NULL,          // TR::vdRegStore

/*
 *END OF OPCODES REQUIRED BY OMR
 */
   NULL,          // TR::iuconst
   NULL,          // TR::luconst
   NULL,          // TR::buconst
   "load",          // TR::iuload
   "load",          // TR::luload
   "load",          // TR::buload
   "load",          // TR::iuloadi
   "load",          // TR::luloadi
   "load",          // TR::buloadi
   "store",          // TR::iustore
   "store",          // TR::lustore
   "store",          // TR::bustore
   "store",          // TR::iustorei
   "store",          // TR::lustorei
   "store",          // TR::bustorei
   "ret",          // TR::iureturn
   "ret",          // TR::lureturn
   NULL,          // TR::iucall
   NULL,          // TR::lucall
   "add",         // TR::iuadd
   "add",         // TR::luadd
   "add",         // TR::buadd
   "sub",         // TR::iusub
   "sub",         // TR::lusub
   "sub",         // TR::busub
   "sub",         // TR::iuneg
   "sub",         // TR::luneg
   "shl",         // TR::iushl
   "shl",         // TR::lushl
   "fptoui",      // TR::f2iu
   "fptoui",      // TR::f2lu
   "fptoui",      // TR::f2bu
   "fptoui",      // TR::f2c
   "fptoui",      // TR::d2iu
   "fptoui",      // TR::d2lu
   "fptoui",      // TR::d2bu
   "fptoui",      // TR::d2c
   NULL,          // TR::iuRegLoad
   NULL,          // TR::luRegLoad
   NULL,          // TR::iuRegStore
   NULL,          // TR::luRegStore
   NULL,          // TR::iuternary
   NULL,          // TR::luternary
   NULL,          // TR::buternary
   NULL,          // TR::suternary
   NULL,          // TR::cconst

   "load",        // TR::cload
   "load",        // TR::cloadi

   "store",       // TR::cstore
   "store",       // TR::cstorei
   NULL,          // TR::monent
   NULL,          // TR::monexit
   NULL,          // TR::monexitfence
   NULL,          // TR::tstart
   NULL,          // TR::tfinish
   NULL,          // TR::tabort

   NULL,          // TR::instanceof
   NULL,          // TR::checkcast
   NULL,          // TR::checkcastAndNULLCHK
   NULL,          // TR::New
   "INVALID",       // TR::newarray
   NULL,          // TR::anewarray
   NULL,          // TR::variableNew
   NULL,          // TR::variableNewArray
   NULL,          // TR::multianewarray
   NULL,          // TR::arraylength
   NULL,          // TR::contigarraylength
   NULL,          // TR::discontigarraylength
   NULL,          // TR::icalli
   NULL,          // TR::iucalli
   NULL,          // TR::lcalli
   NULL,          // TR::lucalli
   NULL,          // TR::fcalli
   NULL,          // TR::dcalli
   NULL,          // TR::acalli
   NULL,          // TR::calli
   NULL,          // TR::fence
   NULL,          // TR::luaddh
   "add",        // TR::cadd

  "getelementptr",          // TR::aiadd
  "getelementptr",          // TR::aiuadd
  "getelementptr",          // TR::aladd
  "getelementptr",          // TR::aluadd

   NULL,          // TR::lusubh
   "sub",         // TR::csub

   NULL,          // TR::imulh, implemented but this table value is unused
   NULL,          // TR::iumulh, implemented but this table value is unused
   NULL,          // TR::lmulh, implemented but this table value is unused
   NULL,          // TR::lumulh, implemented but this table value is unused


   "bitcast",     // TR::ibits2f
   "bitcast",     // TR::fbits2i
   "bitcast",     // TR::lbits2d
   "bitcast",     // TR::dbits2l

   "switch",      // TR::lookup
   NULL,          // TR::trtLookup
   NULL,          // TR::Case, implemented but this table value is unused
   "switch",      // TR::table
   NULL,          // TR::exceptionRangeFence
   NULL,          // TR::dbgFence
   NULL,          // TR::NULLCHK
   NULL,          // TR::ResolveCHK
   NULL,          // TR::ResolveAndNULLCHK
   NULL,          // TR::DIVCHK
   NULL,          // TR::Overflow
   NULL,          // TR::UnsignedOverflowCHK
   NULL,          // TR::BCDCHK
   NULL,          // TR::BNDCHK
   NULL,          // TR::ArrayCopyBNDCHK
   NULL,          // TR::BNDCHKwithSpineCHK
   NULL,          // TR::SpineCHK
   NULL,          // TR::ArrayStoreCHK
   NULL,          // TR::ArrayCHK
   NULL,          // TR::Ret
   NULL,          // TR::arraycopy
   NULL,          // TR::arrayset
   NULL,          // TR::arraytranslate
   NULL,          // TR::arraytranslateAndTest
   NULL,          // TR::countDigits
   NULL,          // TR::long2String
   NULL,          // TR::bitOpMem
   NULL,          // TR::bitOpMemND
   NULL,          // TR::arraycmp
   NULL,          // TR::arraycmpWithPad
   NULL,          // TR::allocationFence
   NULL,          // TR::loadFence
   NULL,          // TR::storeFence
   NULL,          // TR::fullFence
   NULL,          // TR::MergeNew
   NULL,          // TR::computeCC
   NULL,          // TR::butest
   NULL,          // TR::sutest
   NULL,          // TR::bucmp
   NULL,          // TR::bcmp
   NULL,          // TR::sucmp
   NULL,          // TR::scmp
   NULL,          // TR::iucmp
   NULL,          // TR::icmp
   NULL,          // TR::lucmp
   NULL,          // TR::ificmpo
   NULL,          // TR::ificmpno
   NULL,          // TR::iflcmpo
   NULL,          // TR::iflcmpno
   NULL,          // TR::ificmno
   NULL,          // TR::ificmnno
   NULL,          // TR::iflcmno
   NULL,          // TR::iflcmnno
   NULL,          // TR::iuaddc
   NULL,          // TR::luaddc
   NULL,          // TR::iusubb
   NULL,          // TR::lusubb
   NULL,          // TR::icmpset
   NULL,          // TR::lcmpset
   NULL,          // TR::bztestnset
   NULL,          // TR::ibatomicor
   NULL,          // TR::isatomicor
   NULL,          // TR::iiatomicor
   NULL,          // TR::ilatomicor
   NULL,          // TR::dexp
   NULL,          // TR::branch
   NULL,          // TR::igoto

   NULL,          // TR::bexp
   NULL,          // TR::buexp
   NULL,          // TR::sexp
   NULL,          // TR::cexp
   NULL,          // TR::iexp
   NULL,          // TR::iuexp
   NULL,          // TR::lexp
   NULL,          // TR::luexp
   NULL,          // TR::fexp
   NULL,          // TR::fuexp
   NULL,          // TR::duexp

   NULL,          // TR::ixfrs
   NULL,          // TR::lxfrs
   NULL,          // TR::fxfrs
   NULL,          // TR::dxfrs

   NULL,          // TR::fint
   NULL,          // TR::dint
   NULL,          // TR::fnint
   NULL,          // TR::dnint

   NULL,          // TR::fsqrt
   NULL,          // TR::dsqrt

   NULL,          // TR::getstack
   NULL,          // TR::dealloca

   NULL,          // TR::ishfl
   NULL,          // TR::lshfl
   NULL,          // TR::iushfl
   NULL,          // TR::lushfl
   NULL,          // TR::bshfl
   NULL,          // TR::sshfl
   NULL,          // TR::bushfl
   NULL,          // TR::sushfl

   NULL,          // TR::idoz

   NULL,          // TR::dcos
   NULL,          // TR::dsin
   NULL,          // TR::dtan

   NULL,          // TR::dcosh
   NULL,          // TR::dsinh
   NULL,          // TR::dtanh

   NULL,          // TR::dacos
   NULL,          // TR::dasin
   NULL,          // TR::datan

   NULL,          // TR::datan2

   NULL,          // TR::dlog



   NULL,          // TR::imulover

   NULL,          // TR::dfloor
   NULL,          // TR::ffloor
   NULL,          // TR::dceil
   NULL,          // TR::fceil
   NULL,          // TR::ibranch
   NULL,          // TR::mbranch
   NULL,          // TR::getpm
   NULL,          // TR::setpm
   NULL,          // TR::loadAutoOffset


   NULL,          // TR::imax
   NULL,          // TR::iumax
   NULL,          // TR::lmax
   NULL,          // TR::lumax
   NULL,          // TR::fmax
   NULL,          // TR::dmax

   NULL,          // TR::imin
   NULL,          // TR::iumin
   NULL,          // TR::lmin
   NULL,          // TR::lumin
   NULL,          // TR::fmin
   NULL,          // TR::dmin

   NULL,          // TR::trt
   NULL,          // TR::trtSimple


   NULL,          // TR::ihbit,
   NULL,          // TR::ilbit,
   NULL,          // TR::inolz,
   NULL,          // TR::inotz,
   NULL,          // TR::ipopcnt,

   NULL,          // TR::lhbit,
   NULL,          // TR::llbit,
   NULL,          // TR::lnolz,
   NULL,          // TR::lnotz,
   NULL,          // TR::lpopcnt,
   NULL,          // TR::ibyteswap,

   NULL,          // TR::bbitpermute,
   NULL,          // TR::sbitpermute,
   NULL,          // TR::ibitpermute,
   NULL,          // TR::lbitpermute,

   NULL,          // TR::Prefetch

/*
 *  J9 specific opcodes
 */

   NULL,          // TR::dfconst
   NULL,          // TR::ddconst
   NULL,          // TR::deconst
   NULL,          // TR::dfload
   NULL,          // TR::ddload
   NULL,          // TR::deload
   NULL,          // TR::dfloadi
   NULL,          // TR::ddloadi
   NULL,          // TR::deloadi
   NULL,          // TR::dfstore
   NULL,          // TR::ddstore
   NULL,          // TR::destore
   NULL,          // TR::dfstorei
   NULL,          // TR::ddstorei
   NULL,          // TR::destorei
   NULL,          // TR::dfreturn
   NULL,          // TR::ddreturn
   NULL,          // TR::dereturn
   NULL,          // TR::dfcall
   NULL,          // TR::ddcall
   NULL,          // TR::decall
   NULL,          // TR::dfcalli
   NULL,          // TR::ddcalli
   NULL,          // TR::decalli
   NULL,          // TR::dfadd
   NULL,          // TR::ddadd
   NULL,          // TR::deadd
   NULL,          // TR::dfsub
   NULL,          // TR::ddsub
   NULL,          // TR::desub
   NULL,          // TR::dfmul
   NULL,          // TR::ddmul
   NULL,          // TR::demul
   NULL,          // TR::dfdiv
   NULL,          // TR::dddiv
   NULL,          // TR::dediv
   NULL,          // TR::dfrem
   NULL,          // TR::ddrem
   NULL,          // TR::derem
   NULL,          // TR::dfneg
   NULL,          // TR::ddneg
   NULL,          // TR::deneg
   NULL,          // TR::dfabs
   NULL,          // TR::ddabs
   NULL,          // TR::deabs
   NULL,          // TR::dfshl
   NULL,          // TR::dfshr
   NULL,          // TR::ddshl
   NULL,          // TR::ddshr
   NULL,          // TR::deshl
   NULL,          // TR::deshr
   NULL,          // TR::dfshrRounded
   NULL,          // TR::ddshrRounded
   NULL,          // TR::deshrRounded
   NULL,          // TR::dfSetNegative
   NULL,          // TR::ddSetNegative
   NULL,          // TR::deSetNegative
   NULL,          // TR::dfModifyPrecision
   NULL,          // TR::ddModifyPrecision
   NULL,          // TR::deModifyPrecision

   NULL,          // TR::i2df
   NULL,          // TR::iu2df
   NULL,          // TR::l2df
   NULL,          // TR::lu2df
   NULL,          // TR::f2df
   NULL,          // TR::d2df
   NULL,          // TR::dd2df
   NULL,          // TR::de2df
   NULL,          // TR::b2df
   NULL,          // TR::bu2df
   NULL,          // TR::s2df
   NULL,          // TR::su2df

   NULL,          // TR::df2i
   NULL,          // TR::df2iu
   NULL,          // TR::df2l
   NULL,          // TR::df2lu
   NULL,          // TR::df2f
   NULL,          // TR::df2d
   NULL,          // TR::df2dd
   NULL,          // TR::df2de
   NULL,          // TR::df2b
   NULL,          // TR::df2bu
   NULL,          // TR::df2s
   NULL,          // TR::df2c

   NULL,          // TR::i2dd
   NULL,          // TR::iu2dd
   NULL,          // TR::l2dd
   NULL,          // TR::lu2dd
   NULL,          // TR::f2dd
   NULL,          // TR::d2dd
   NULL,          // TR::de2dd
   NULL,          // TR::b2dd
   NULL,          // TR::bu2dd
   NULL,          // TR::s2dd
   NULL,          // TR::su2dd

   NULL,          // TR::dd2i
   NULL,          // TR::dd2iu
   NULL,          // TR::dd2l
   NULL,          // TR::dd2lu
   NULL,          // TR::dd2f
   NULL,          // TR::dd2d
   NULL,          // TR::dd2de
   NULL,          // TR::dd2b
   NULL,          // TR::dd2bu
   NULL,          // TR::dd2s
   NULL,          // TR::dd2c

   NULL,          // TR::i2de
   NULL,          // TR::iu2de
   NULL,          // TR::l2de
   NULL,          // TR::lu2de
   NULL,          // TR::f2de
   NULL,          // TR::d2de
   NULL,          // TR::b2de
   NULL,          // TR::bu2de
   NULL,          // TR::s2de
   NULL,          // TR::su2de

   NULL,          // TR::de2i
   NULL,          // TR::de2iu
   NULL,          // TR::de2l
   NULL,          // TR::de2lu
   NULL,          // TR::de2f
   NULL,          // TR::de2d
   NULL,          // TR::de2b
   NULL,          // TR::de2bu
   NULL,          // TR::de2s
   NULL,          // TR::de2c

   NULL,          // TR::ifdfcmpeq
   NULL,          // TR::ifdfcmpne
   NULL,          // TR::ifdfcmplt
   NULL,          // TR::ifdfcmpge
   NULL,          // TR::ifdfcmpgt
   NULL,          // TR::ifdfcmple
   NULL,          // TR::ifdfcmpequ
   NULL,          // TR::ifdfcmpneu
   NULL,          // TR::ifdfcmpltu
   NULL,          // TR::ifdfcmpgeu
   NULL,          // TR::ifdfcmpgtu
   NULL,          // TR::ifdfcmpleu

   NULL,          // TR::dfcmpeq
   NULL,          // TR::dfcmpne
   NULL,          // TR::dfcmplt
   NULL,          // TR::dfcmpge
   NULL,          // TR::dfcmpgt
   NULL,          // TR::dfcmple
   NULL,          // TR::dfcmpequ
   NULL,          // TR::dfcmpneu
   NULL,          // TR::dfcmpltu
   NULL,          // TR::dfcmpgeu
   NULL,          // TR::dfcmpgtu
   NULL,          // TR::dfcmpleu

   NULL,          // TR::ifddcmpeq
   NULL,          // TR::ifddcmpne
   NULL,          // TR::ifddcmplt
   NULL,          // TR::ifddcmpge
   NULL,          // TR::ifddcmpgt
   NULL,          // TR::ifddcmple
   NULL,          // TR::ifddcmpequ
   NULL,          // TR::ifddcmpneu
   NULL,          // TR::ifddcmpltu
   NULL,          // TR::ifddcmpgeu
   NULL,          // TR::ifddcmpgtu
   NULL,          // TR::ifddcmpleu

   NULL,          // TR::ddcmpeq
   NULL,          // TR::ddcmpne
   NULL,          // TR::ddcmplt
   NULL,          // TR::ddcmpge
   NULL,          // TR::ddcmpgt
   NULL,          // TR::ddcmple
   NULL,          // TR::ddcmpequ
   NULL,          // TR::ddcmpneu
   NULL,          // TR::ddcmpltu
   NULL,          // TR::ddcmpgeu
   NULL,          // TR::ddcmpgtu
   NULL,          // TR::ddcmpleu

   NULL,          // TR::ifdecmpeq
   NULL,          // TR::ifdecmpne
   NULL,          // TR::ifdecmplt
   NULL,          // TR::ifdecmpge
   NULL,          // TR::ifdecmpgt
   NULL,          // TR::ifdecmple
   NULL,          // TR::ifdecmpequ
   NULL,          // TR::ifdecmpneu
   NULL,          // TR::ifdecmpltu
   NULL,          // TR::ifdecmpgeu
   NULL,          // TR::ifdecmpgtu
   NULL,          // TR::ifdecmpleu

   NULL,          // TR::decmpeq
   NULL,          // TR::decmpne
   NULL,          // TR::decmplt
   NULL,          // TR::decmpge
   NULL,          // TR::decmpgt
   NULL,          // TR::decmple
   NULL,          // TR::decmpequ
   NULL,          // TR::decmpneu
   NULL,          // TR::decmpltu
   NULL,          // TR::decmpgeu
   NULL,          // TR::decmpgtu
   NULL,          // TR::decmpleu

   NULL,          // TR::dfRegLoad
   NULL,          // TR::ddRegLoad
   NULL,          // TR::deRegLoad
   NULL,          // TR::dfRegStore
   NULL,          // TR::ddRegStore
   NULL,          // TR::deRegStore

   NULL,          // TR::dfternary
   NULL,          // TR::ddternary
   NULL,          // TR::deternary

   NULL,          // TR::dfexp
   NULL,          // TR::ddexp
   NULL,          // TR::deexp
   NULL,          // TR::dfnint
   NULL,          // TR::ddnint
   NULL,          // TR::denint
   NULL,          // TR::dfsqrt
   NULL,          // TR::ddsqrt
   NULL,          // TR::desqrt

   NULL,          // TR::dfcos
   NULL,          // TR::ddcos
   NULL,          // TR::decos
   NULL,          // TR::dfsin
   NULL,          // TR::ddsin
   NULL,          // TR::desin
   NULL,          // TR::dftan
   NULL,          // TR::ddtan
   NULL,          // TR::detan

   NULL,          // TR::dfcosh
   NULL,          // TR::ddcosh
   NULL,          // TR::decosh
   NULL,          // TR::dfsinh
   NULL,          // TR::ddsinh
   NULL,          // TR::desinh
   NULL,          // TR::dftanh
   NULL,          // TR::ddtanh
   NULL,          // TR::detanh

   NULL,          // TR::dfacos
   NULL,          // TR::ddacos
   NULL,          // TR::deacos
   NULL,          // TR::dfasin
   NULL,          // TR::ddasin
   NULL,          // TR::deasin
   NULL,          // TR::dfatan
   NULL,          // TR::ddatan
   NULL,          // TR::deatan

   NULL,          // TR::dfatan2
   NULL,          // TR::ddatan2
   NULL,          // TR::deatan2
   NULL,          // TR::dflog
   NULL,          // TR::ddlog
   NULL,          // TR::delog
   NULL,          // TR::dffloor
   NULL,          // TR::ddfloor
   NULL,          // TR::defloor
   NULL,          // TR::dfceil
   NULL,          // TR::ddceil
   NULL,          // TR::deceil
   NULL,          // TR::dfmax
   NULL,          // TR::ddmax
   NULL,          // TR::demax
   NULL,          // TR::dfmin
   NULL,          // TR::ddmin
   NULL,          // TR::demin

   NULL,          // TR::dfInsExp
   NULL,          // TR::ddInsExp
   NULL,          // TR::deInsExp

   NULL,          // TR::ddclean
   NULL,          // TR::declean

   NULL,          // TR::zdload
   NULL,          // TR::zdloadi
   NULL,          // TR::zdstore
   NULL,          // TR::zdstorei

   NULL,          // TR::pd2zd
   NULL,          // TR::zd2pd

   NULL,          // TR::zdsleLoad
   NULL,          // TR::zdslsLoad
   NULL,          // TR::zdstsLoad

   NULL,          // TR::zdsleLoadi
   NULL,          // TR::zdslsLoadi
   NULL,          // TR::zdstsLoadi

   NULL,          // TR::zdsleStore
   NULL,          // TR::zdslsStore
   NULL,          // TR::zdstsStore

   NULL,          // TR::zdsleStorei
   NULL,          // TR::zdslsStorei
   NULL,          // TR::zdstsStorei

   NULL,          // TR::zd2zdsle
   NULL,          // TR::zd2zdsls
   NULL,          // TR::zd2zdsts

   NULL,          // TR::zdsle2pd
   NULL,          // TR::zdsls2pd
   NULL,          // TR::zdsts2pd

   NULL,          // TR::zdsle2zd
   NULL,          // TR::zdsls2zd
   NULL,          // TR::zdsts2zd

   NULL,          // TR::pd2zdsls
   NULL,          // TR::pd2zdslsSetSign
   NULL,          // TR::pd2zdsts
   NULL,          // TR::pd2zdstsSetSign

   NULL,          // TR::zd2df
   NULL,          // TR::df2zd
   NULL,          // TR::zd2dd
   NULL,          // TR::dd2zd
   NULL,          // TR::zd2de
   NULL,          // TR::de2zd

   NULL,          // TR::zd2dfAbs
   NULL,          // TR::zd2ddAbs
   NULL,          // TR::zd2deAbs

   NULL,          // TR::df2zdSetSign
   NULL,          // TR::dd2zdSetSign
   NULL,          // TR::de2zdSetSign

   NULL,          // TR::df2zdClean
   NULL,          // TR::dd2zdClean
   NULL,          // TR::de2zdClean

   NULL,          // TR::udLoad
   NULL,          // TR::udslLoad
   NULL,          // TR::udstLoad

   NULL,          // TR::udLoadi
   NULL,          // TR::udslLoadi
   NULL,          // TR::udstLoadi

   NULL,          // TR::udStore
   NULL,          // TR::udslStore
   NULL,          // TR::udstStore

   NULL,          // TR::udStorei
   NULL,          // TR::udslStorei
   NULL,          // TR::udstStorei

   NULL,          // TR::pd2ud
   NULL,          // TR::pd2udsl
   NULL,          // TR::pd2udst

   NULL,          // TR::udsl2ud
   NULL,          // TR::udst2ud

   NULL,          // TR::ud2pd
   NULL,          // TR::udsl2pd
   NULL,          // TR::udst2pd

   NULL,          // TR::pdload
   NULL,          // TR::pdloadi
   NULL,          // TR::pdstore
   NULL,          // TR::pdstorei
   NULL,          // TR::pdadd
   NULL,          // TR::pdsub
   NULL,          // TR::pdmul
   NULL,          // TR::pddiv
   NULL,          // TR::pdrem
   NULL,          // TR::pdneg
   NULL,          // TR::pdabs
   NULL,          // TR::pdshr
   NULL,          // TR::pdshl
   NULL,          // TR::pdshrSetSign
   NULL,          // TR::pdshlSetSign
   NULL,          // TR::pdshlOverflow
   NULL,          // TR::pdchk
   NULL,          // TR::pd2i
   NULL,          // TR::pd2iOverflow
   NULL,          // TR::pd2iu
   NULL,          // TR::i2pd
   NULL,          // TR::iu2pd
   NULL,          // TR::pd2l
   NULL,          // TR::pd2lOverflow
   NULL,          // TR::pd2lu
   NULL,          // TR::l2pd
   NULL,          // TR::lu2pd
   NULL,          // TR::pd2f
   NULL,          // TR::pd2d
   NULL,          // TR::f2pd
   NULL,          // TR::d2pd
   NULL,          // TR::pdcmpeq
   NULL,          // TR::pdcmpne
   NULL,          // TR::pdcmplt
   NULL,          // TR::pdcmpge
   NULL,          // TR::pdcmpgt
   NULL,          // TR::pdcmple
   NULL,          // TR::pdclean
   NULL,          // TR::pdclear
   NULL,          // TR::pdclearSetSign
   NULL,          // TR::pdSetSign
   NULL,          // TR::pdModifyPrecision
   NULL,          // TR::pd2df
   NULL,          // TR::pd2dfAbs
   NULL,          // TR::df2pd
   NULL,          // TR::df2pdSetSign
   NULL,          // TR::df2pdClean
   NULL,          // TR::pd2dd
   NULL,          // TR::pd2ddAbs
   NULL,          // TR::dd2pd
   NULL,          // TR::dd2pdSetSign
   NULL,          // TR::dd2pdClean
   NULL,          // TR::pd2de
   NULL,          // TR::pd2deAbs
   NULL,          // TR::de2pd
   NULL,          // TR::de2pdSetSign
   NULL,          // TR::de2pdClean
   NULL,          // TR::ircload
   NULL,          // TR::irsload
   NULL,          // TR::iruiload
   NULL,          // TR::iriload
   NULL,          // TR::irulload
   NULL,          // TR::irlload
   NULL,          // TR::irsstore
   NULL,          // TR::iristore
   NULL,          // TR::irlstore
};

static_assert(sizeof(nvvmOpCodeNames) == (TR::NumIlOps*sizeof(char*)), "Number of elements in nvvmOpCodeNames does not match the value of TR::NumIlOps");

static const char* getOpCodeName(TR::ILOpCodes opcode) {

   TR_ASSERT(opcode < TR::NumIlOps, "Wrong opcode");
   return nvvmOpCodeNames[opcode];

}


char *nvvmTypeNames[TR::NumTypes] =
   {
   "void",    // "TR::NoType"
   "i8",      // "TR::Int8"
   "i16",     // "TR::Int16"
   "i32",     // "TR::Int32"
   "i64",     // "TR::Int64"
   "float",   // "TR::Float"
   "double",  // "TR::Double"
   "i8*"      // "TR::Address"
   };

static const char* getTypeName(TR::DataType type) {
    if (type >= TR::NoType && type <= TR::Address)
       {
       return nvvmTypeNames[type];
       }
    else
       {
       TR_ASSERT(false, "Unsupported type");
       return "???";
       }
}

char *nvvmVarTypeNames[TR::NumTypes] =
   {
   "void",    // "TR::NoType"
   "i8",      // "TR::Int8"
   "i16",     // "TR::Int16"
   "i32",     // "TR::Int32"
   "i64",     // "TR::Int64"
   "f32",     // "TR::Float"
   "f64",     // "TR::Double"
   "p64"      // "TR::Address"
   };

static const char* getVarTypeName(TR::DataType type) {
    if (type >= TR::NoType && type <= TR::Address)
       {
       return nvvmVarTypeNames[type];
       }
    else
       {
       TR_ASSERT(false, "Unsupported type");
       return "???";
       }
}

#define MAX_NAME 256


static void getParmName(int32_t slot, char * s, bool addr = true)
   {
   int32_t len = 0;

   len = snprintf(s, MAX_NAME, "%%p%d%s", slot,  addr ? ".addr" : "");

   TR_ASSERT(len < MAX_NAME, "Auto's or parm's name is too long\n");
   }


static void getAutoOrParmName(TR::Symbol *sym, char * s, bool addr = true)
   {
   int32_t len = 0;
   TR_ASSERT(sym->isAutoOrParm(), "expecting auto or parm");

   if (sym->isParm())
      len = snprintf(s, MAX_NAME, "%%p%d%s", sym->castToParmSymbol()->getSlot(),  addr ? ".addr" : "");
   else
      len = snprintf(s, MAX_NAME, "%%a%d%s", sym->castToAutoSymbol()->getLiveLocalIndex(), addr ? ".addr" : "");

   TR_ASSERT(len < MAX_NAME, "Auto's or parm's name is too long\n");
   }


#define INIT_BUFFER_SIZE 65535

class NVVMIRBuffer
   {
   public:
   NVVMIRBuffer(TR_Memory* mem)
      {
      m = mem;
      size = INIT_BUFFER_SIZE;
      buffer = (char*)m->allocateHeapMemory(size);
      s = buffer;
      }
   void print(char *format, ...)
      {
      va_list args;
      va_start (args, format);
      int32_t left = size - (s - buffer);

      va_list args_copy;
      va_copy(args_copy, args);
      int32_t len = vsnprintf(s, left, format, args_copy);
      va_copy_end(args_copy);

      if ((len + 1) > left)
         {
         expand(len + 1 - left);
         left = size - (s - buffer);
         len = vsnprintf(s, left, format, args);
         }

      s += len;
      va_end(args);
      }

   char * getString() { return buffer; }

   private:

   void expand(int32_t min)
      {
      size += (min >= size) ? size*2 : size;

      char * newBuffer = (char*)m->allocateHeapMemory(size);
      memcpy(newBuffer, buffer, s - buffer);
      s = newBuffer + (s - buffer);
      buffer = newBuffer;
      }

   char *buffer;
   char *s;
   int32_t size;
   TR_Memory* m;
};


static void getNodeName(TR::Node* node, char * s, TR::Compilation *comp)
   {
   int32_t len = 0;
   if (node->getOpCode().isLoadConst())
      {
      bool isUnsigned = node->getOpCode().isUnsigned();
      switch (node->getDataType())
         {
         case TR::Int8:
            if(isUnsigned)
               len = snprintf(s, MAX_NAME, "%u", node->getUnsignedByte());
            else
               len = snprintf(s, MAX_NAME, "%d", node->getByte());
            break;
         case TR::Int16:
            len = snprintf(s, MAX_NAME, "%d", node->getConst<uint16_t>());
            break;
         case TR::Int32:
            if(isUnsigned)
               len = snprintf(s, MAX_NAME, "%u", node->getUnsignedInt());
            else
               len = snprintf(s, MAX_NAME, "%d", node->getInt());
            break;
         case TR::Int64:
            if(isUnsigned)
               len = snprintf(s, MAX_NAME, UINT64_PRINTF_FORMAT, node->getUnsignedLongInt());
            else
               len = snprintf(s, MAX_NAME, INT64_PRINTF_FORMAT, node->getLongInt());
            break;
         case TR::Float:
            union
               {
               double                  doubleValue;
               int64_t                 doubleBits;
               };
            doubleValue = node->getFloat();
            len = snprintf(s, MAX_NAME, "0x%0.16llx", doubleBits);
            break;
         case TR::Double:
            len = snprintf(s, MAX_NAME, "0x%0.16llx", node->getDoubleBits());
            break;
         case TR::Address:
            if (node->getAddress() == 0)
               len = snprintf(s, MAX_NAME, "null");
            else
               TR_ASSERT(0, "Non-null Address constants should not occur.\n");
            break;
         default:
            TR_ASSERT(0, "Unknown/unimplemented data type\n");
         }
      }
   else
      {
      len = snprintf(s, MAX_NAME, "%%%d", node->getLocalIndex());
      }

   TR_ASSERT(len < MAX_NAME, "Node's name is too long\n");
   }

char* getNVVMMathFunctionName(TR::Node *node)
   {
   switch (((TR::MethodSymbol*)node->getSymbolReference()->getSymbol())->getRecognizedMethod())
      {
      case TR::java_lang_Math_sqrt:
         return "sqrt";
      case TR::java_lang_Math_sin:
      case TR::java_lang_StrictMath_sin:
         return "sin";
      case TR::java_lang_Math_cos:
      case TR::java_lang_StrictMath_cos:
         return "cos";
      case TR::java_lang_Math_log:
      case TR::java_lang_StrictMath_log:
         return "log";
      case TR::java_lang_Math_exp:
      case TR::java_lang_StrictMath_exp:
         return "exp";
      case TR::java_lang_Math_abs_F:
         return "fabsf";
      case TR::java_lang_Math_abs_D:
         return "fabs";
      default:
         return "ERROR";
      }
   return "ERROR";
   }

bool J9::CodeGenerator::handleRecognizedMethod(TR::Node *node, NVVMIRBuffer &ir, TR::Compilation *comp)
   {
   char name0[MAX_NAME];
   switch (((TR::MethodSymbol*)node->getSymbolReference()->getSymbol())->getRecognizedMethod())
      {
      case TR::com_ibm_gpu_Kernel_blockIdxX:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.x()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_blockIdxY:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.y()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_blockIdxZ:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.z()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_blockDimX:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ntid.x()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_blockDimY:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ntid.y()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_blockDimZ:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ntid.z()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_threadIdxX:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.tid.x()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_threadIdxY:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.tid.y()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_threadIdxZ:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.tid.z()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_syncThreads:
          ir.print("  call void @llvm.nvvm.barrier0()\n");
          node->setLocalIndex(_gpuNodeCount--);
          break;
      case TR::java_lang_Math_sqrt:
      case TR::java_lang_Math_sin:
      case TR::java_lang_Math_cos:
      case TR::java_lang_Math_log:
      case TR::java_lang_Math_exp:
      case TR::java_lang_Math_abs_D:
          if (!comp->getOptions()->getEnableGPU(TR_EnableGPUEnableMath)) return false;
          getNodeName(node->getChild(0), name0, comp);
          ir.print("  %%%d = call double @__nv_%s(double %s)\n", node->getLocalIndex(), getNVVMMathFunctionName(node), name0);
          break;
      case TR::java_lang_StrictMath_sin:
      case TR::java_lang_StrictMath_cos:
      case TR::java_lang_StrictMath_log:
      case TR::java_lang_StrictMath_exp:
          if (!comp->getOptions()->getEnableGPU(TR_EnableGPUEnableMath)) return false;
          getNodeName(node->getChild(1), name0, comp);
          ir.print("  %%%d = call double @__nv_%s(double %s)\n", node->getLocalIndex(), getNVVMMathFunctionName(node), name0);
          break;
      case TR::java_lang_Math_abs_F:
          if (!comp->getOptions()->getEnableGPU(TR_EnableGPUEnableMath)) return false;
          getNodeName(node->getChild(0), name0, comp);
          ir.print("  %%%d = call float @__nv_%s(float %s)\n", node->getLocalIndex(), getNVVMMathFunctionName(node), name0);
          break;
      default:
          return false;
      }
   return true;
   }


bool J9::CodeGenerator::handleRecognizedField(TR::Node *node, NVVMIRBuffer &ir)
   {
   switch (node->getSymbolReference()->getSymbol()->getRecognizedField())
      {
      case TR::Symbol::Com_ibm_gpu_Kernel_blockIdxX:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.x()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_blockIdxY:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.y()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_blockIdxZ:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.z()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_blockDimX:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ntid.x()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_blockDimY:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ntid.y()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_blockDimZ:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ntid.z()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_threadIdxX:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.tid.x()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_threadIdxY:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.tid.y()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_threadIdxZ:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.tid.z()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_syncThreads:
          ir.print("  call void @llvm.nvvm.barrier0()\n");
          node->setLocalIndex(_gpuNodeCount--);
          break;
      default:
          return false;
      }
      return true;
   }

void J9::CodeGenerator::printArrayCopyNVVMIR(TR::Node *node, NVVMIRBuffer &ir, TR::Compilation *comp)
   {
   //Some forms of array copy have five children. First two nodes are used for write barriers which we don't need
   // Three child version
   //      child 0 ------  Source byte address;
   //      child 1 ------  Destination byte address;
   //      child 2 ------  Copy length in byte;
   // Five child version:
   //      child 0 ------  Source array object; (skipped)
   //      child 1 ------  Destination array object; (skipped)
   //      child 2 ------  Source byte address;
   //      child 3 ------  Destination byte address;
   //      child 4 ------  Copy length in byte;
   //childrenNodeOffset is set such that we access Source byte address, Destination byte address and Copy length.
   int childrenNodeOffset = node->getNumChildren() == 5 ? 2 : 0;

   char name0[MAX_NAME], name1[MAX_NAME], name2[MAX_NAME];
   getNodeName(node->getChild(0+childrenNodeOffset), name0, comp);
   getNodeName(node->getChild(1+childrenNodeOffset), name1, comp);
   getNodeName(node->getChild(2+childrenNodeOffset), name2, comp);

   int arrayCopyID = node->getLocalIndex();
   bool isWordCopy = node->chkWordElementArrayCopy();
   bool isHalfwordCopy = node->chkHalfWordElementArrayCopy();
   bool unknownCopy = !(isWordCopy || isHalfwordCopy);
   bool isBackwardsCopy = !node->isForwardArrayCopy();
   bool is64bitCopyLength = (node->getChild(2+childrenNodeOffset)->getDataType() == TR::Int64);

   /* Example NVVM IR:

   ; Inputs to the array copy that come from the children:
     %8 = getelementptr inbounds i8* %7, i64 76  ; Source addr
     %10 = getelementptr inbounds i8* %9, i64 76 ; Destination addr
     %14 = mul i64 %13, 2                        ; Copy Length in bytes

   ; Generated ArrayCopy NVVM IR
   ; This is a reverse halfword array copy
     br label %ArrayCopy15
   ArrayCopy15:
     %15 = ptrtoint i8* %8 to i64  ; Generated for reverse array copy.
     %16 = ptrtoint i8* %10 to i64 ; Changes source and destination
     %17 = add i64 %15, %14        ; to point to the end of the array
     %18 = add i64 %16, %14        ; These lines are not generated
     %19 = sub i64 %17, 2          ; for a forward array copy
     %20 = sub i64 %18, 2          ;
     %21 = inttoptr i64 %19 to i8* ;
     %22 = inttoptr i64 %20 to i8* ;
     br label %ArrayCopyHeader15
   ArrayCopyHeader15:
     %23 = phi i64 [ %14, %ArrayCopy15 ], [ %36, %ArrayCopyBody15 ] ; Phi nodes save a different value to the temp
     %24 = phi i8* [ %21, %ArrayCopy15 ], [ %34, %ArrayCopyBody15 ] ; based on the name of the previous block before
     %25 = phi i8* [ %22, %ArrayCopy15 ], [ %35, %ArrayCopyBody15 ] ; jumping to ArrayCopyHeader15
     %26 = bitcast i8* %24 to i16*
     %27 = bitcast i8* %25 to i16*
     %28 = icmp sle i64 %23, 0
     br i1 %28, label %AfterArrayCopy15, label %ArrayCopyBody15     ; branch to exit if no more work to do
   ArrayCopyBody15:
     %29 = load i16* %26              ; load data from input array
     store i16 %29, i16* %27          ; store data to output array
     %30 = ptrtoint i16* %26 to i64
     %31 = ptrtoint i16* %27 to i64
     %32 = sub i64 %30, 2             ; sub is used for reverse copy, add used for forward copy
     %33 = sub i64 %31, 2             ; sub is used for reverse copy, add used for forward copy
     %34 = inttoptr i64 %32 to i8*
     %35 = inttoptr i64 %33 to i8*
     %36 = sub i64 %23, 2             ; decrement copy length
     br label %ArrayCopyHeader15
   AfterArrayCopy15:
   */


   //create a new block so the phi nodes know the name of the preceding block
   ir.print("  br label %%ArrayCopy%d\n", arrayCopyID);
   ir.print("ArrayCopy%d:\n", arrayCopyID);

   //for a backwards copy, the source and destination pointers need to be adjusted to
   //point to the last element.
   if (isBackwardsCopy)
      {
      if (!is64bitCopyLength)
         {
         ir.print("  %%%d = sext %s %s to i64\n",
                      node->getLocalIndex(),
                      getTypeName(node->getChild(2+childrenNodeOffset)->getDataType()),
                      name2);
         node->setLocalIndex(_gpuNodeCount++);
         }

      ir.print("  %%%d = ptrtoint %s %s to i64\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()),
                   name0);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = ptrtoint %s %s to i64\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()),
                   name1);
      node->setLocalIndex(_gpuNodeCount++);

      if (is64bitCopyLength)
         {
         ir.print("  %%%d = add i64 %%%d, %s\n",
                      node->getLocalIndex(),
                      node->getLocalIndex()-2,
                      name2);
         node->setLocalIndex(_gpuNodeCount++);

         ir.print("  %%%d = add i64 %%%d, %s\n",
                      node->getLocalIndex(),
                      node->getLocalIndex()-2,
                      name2);
         node->setLocalIndex(_gpuNodeCount++);
         }
      else
         {
         ir.print("  %%%d = add i64 %%%d, %%%d\n",
                      node->getLocalIndex(),
                      node->getLocalIndex()-2,
                      node->getLocalIndex()-3);
         node->setLocalIndex(_gpuNodeCount++);

         ir.print("  %%%d = add i64 %%%d, %%%d\n",
                      node->getLocalIndex(),
                      node->getLocalIndex()-2,
                      node->getLocalIndex()-4);
         node->setLocalIndex(_gpuNodeCount++);
         }

      ir.print("  %%%d = sub i64 %%%d, %d\n",
                   node->getLocalIndex(),
                   node->getLocalIndex()-2,
                   isWordCopy ? 4 : isHalfwordCopy ? 2 : 1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = sub i64 %%%d, %d\n",
                   node->getLocalIndex(),
                   node->getLocalIndex()-2,
                   isWordCopy ? 4 : isHalfwordCopy ? 2 : 1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = inttoptr i64 %%%d to %s\n",
                   node->getLocalIndex(),
                   node->getLocalIndex()-2,
                   getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()));
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = inttoptr i64 %%%d to %s\n",
                   node->getLocalIndex(),
                   node->getLocalIndex()-2,
                   getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()));
      node->setLocalIndex(_gpuNodeCount++);
      }

   ir.print("  br label %%ArrayCopyHeader%d\n", arrayCopyID);
   ir.print("ArrayCopyHeader%d:\n", arrayCopyID);

   //copy length in bytes
   ir.print("  %%%d = phi %s [ %s, %%ArrayCopy%d ], [ %%%d, %%ArrayCopyBody%d ]\n",
                node->getLocalIndex(),
                getTypeName(node->getChild(2+childrenNodeOffset)->getDataType()),
                name2,
                arrayCopyID,
                unknownCopy ? node->getLocalIndex()+11 : node->getLocalIndex()+13,
                arrayCopyID);
   node->setLocalIndex(_gpuNodeCount++);

   if (!isBackwardsCopy)
      {
      //source address
      ir.print("  %%%d = phi %s [ %s, %%ArrayCopy%d ], [ %%%d, %%ArrayCopyBody%d ]\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()),
                   name0,
                   arrayCopyID,
                   unknownCopy ? node->getLocalIndex()+8 : node->getLocalIndex()+10,
                   arrayCopyID);
      node->setLocalIndex(_gpuNodeCount++);

      //destination address
      ir.print("  %%%d = phi %s [ %s, %%ArrayCopy%d ], [ %%%d, %%ArrayCopyBody%d ]\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()),
                   name1,
                   arrayCopyID,
                   unknownCopy ? node->getLocalIndex()+8 : node->getLocalIndex()+10,
                   arrayCopyID);
      node->setLocalIndex(_gpuNodeCount++);
      }
   else
      {
      //source address
      ir.print("  %%%d = phi %s [ %%%d, %%ArrayCopy%d ], [ %%%d, %%ArrayCopyBody%d ]\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()),
                   node->getLocalIndex()-3,
                   arrayCopyID,
                   unknownCopy ? node->getLocalIndex()+8 : node->getLocalIndex()+10,
                   arrayCopyID);
      node->setLocalIndex(_gpuNodeCount++);

      //destination address
      ir.print("  %%%d = phi %s [ %%%d, %%ArrayCopy%d ], [ %%%d, %%ArrayCopyBody%d ]\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()),
                   node->getLocalIndex()-3,
                   arrayCopyID,
                   unknownCopy ? node->getLocalIndex()+8 : node->getLocalIndex()+10,
                   arrayCopyID);
      node->setLocalIndex(_gpuNodeCount++);
      }

   //change pointer types from i8* if copying halfword or word data
   if (isWordCopy || isHalfwordCopy)
      {
      ir.print("  %%%d = bitcast %s %%%d to %s\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()),
                   node->getLocalIndex()-2,
                   isWordCopy ? "i32*" : "i16*");
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = bitcast %s %%%d to %s\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()),
                   node->getLocalIndex()-2,
                   isWordCopy ? "i32*" : "i16*");
      node->setLocalIndex(_gpuNodeCount++);
      }

   //check if byte length is less than or equal to zero and skip the copy if true
   ir.print("  %%%d = icmp sle %s %%%d, 0\n",
                node->getLocalIndex(),
                getTypeName(node->getChild(2+childrenNodeOffset)->getDataType()),
                unknownCopy ? node->getLocalIndex()-3 : node->getLocalIndex()-5);
   node->setLocalIndex(_gpuNodeCount++);

   ir.print("  br i1 %%%d, label %%AfterArrayCopy%d, label %%ArrayCopyBody%d\n",
                node->getLocalIndex()-1,
                arrayCopyID,
                arrayCopyID);

   ir.print("ArrayCopyBody%d:\n", arrayCopyID);

   //load data to copy
   ir.print("  %%%d = load %s %%%d\n",
                node->getLocalIndex(),
                isWordCopy ? "i32*" : isHalfwordCopy ? "i16*" : getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()),
                node->getLocalIndex()-3);
   node->setLocalIndex(_gpuNodeCount++);

   //store loaded data
   ir.print("  store %s %%%d, %s %%%d\n",
                isWordCopy ? "i32" : isHalfwordCopy ? "i16" : "i8",
                node->getLocalIndex()-1,
                isWordCopy ? "i32*" : isHalfwordCopy ? "i16*" : getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()),
                node->getLocalIndex()-3);

   ir.print("  %%%d = ptrtoint %s %%%d to i64\n",
                node->getLocalIndex(),
                isWordCopy ? "i32*" : isHalfwordCopy ? "i16*" : getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()),
                node->getLocalIndex()-4);
   node->setLocalIndex(_gpuNodeCount++);

   ir.print("  %%%d = ptrtoint %s %%%d to i64\n",
                node->getLocalIndex(),
                isWordCopy ? "i32*" : isHalfwordCopy ? "i16*" : getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()),
                node->getLocalIndex()-4);
   node->setLocalIndex(_gpuNodeCount++);

   //move source pointer
   ir.print("  %%%d = %s i64 %%%d, %d\n",
                node->getLocalIndex(),
                isBackwardsCopy ? "sub" : "add",
                node->getLocalIndex()-2,
                isWordCopy ? 4 : isHalfwordCopy ? 2 : 1);
   node->setLocalIndex(_gpuNodeCount++);

   //move destination pointer
   ir.print("  %%%d = %s i64 %%%d, %d\n",
                node->getLocalIndex(),
                isBackwardsCopy ? "sub" : "add",
                node->getLocalIndex()-2,
                isWordCopy ? 4 : isHalfwordCopy ? 2 : 1);
   node->setLocalIndex(_gpuNodeCount++);

   ir.print("  %%%d = inttoptr i64 %%%d to %s\n",
                node->getLocalIndex(),
                node->getLocalIndex()-2,
                getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()));
   node->setLocalIndex(_gpuNodeCount++);

   ir.print("  %%%d = inttoptr i64 %%%d to %s\n",
                node->getLocalIndex(),
                node->getLocalIndex()-2,
                getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()));
   node->setLocalIndex(_gpuNodeCount++);

   //decrement copy length
   ir.print("  %%%d = sub %s %%%d, %d\n",
                node->getLocalIndex(),
                getTypeName(node->getChild(2+childrenNodeOffset)->getDataType()),
                unknownCopy ? node->getLocalIndex()-11 : node->getLocalIndex()-13,
                isWordCopy ? 4 : isHalfwordCopy ? 2 : 1);

   ir.print("  br label %%ArrayCopyHeader%d\n", arrayCopyID);
   ir.print("AfterArrayCopy%d:\n", arrayCopyID);
   }

bool isThisPointer(TR::SymbolReference * symRef)
   {
   return symRef->getSymbol()->isParm() &&
          ((TR::ParameterSymbol *)symRef->getSymbol())->getSlot() == 0;
   }

char * getTypeNameFromSignature(char* sig, int32_t sigLength)
   {
   TR_ASSERT(sigLength == 2 && sig[0] == '[', "only handling static shared arrays");
   switch (sig[1])
      {
      case 'Z': return "i8";
      case 'B': return "i8";
      case 'C': return "i16";
      case 'S': return "i16";
      case 'I': return "i32";
      case 'J': return "i64";
      case 'F': return "float";
      case 'D': return "double";
      }
   TR_ASSERT(false, "unsupported shared array type\n");
   return NULL;
   }

static bool isSharedMemory(TR::Node *node, TR_SharedMemoryAnnotations *sharedMemory, TR::Compilation *comp)
   {
   if (!comp->isGPUCompilation()) return false;

   TR::SymbolReference *symRef = node->getSymbolReference();
   if (!symRef->getSymbol()->isAutoOrParm() && symRef->getCPIndex() != -1)
      {
      TR_SharedMemoryField field = sharedMemory->find(comp, symRef);
      if (field.getSize() > 0) return true;
      }
   return false;
   }


TR::CodeGenerator::GPUResult
J9::CodeGenerator::printNVVMIR(
      NVVMIRBuffer &ir,
      TR::Node * node,
      TR_RegionStructure *loop,
      TR_BitVector *targetBlocks,
      vcount_t visitCount,
      TR_SharedMemoryAnnotations *sharedMemory,
      int32_t &nextParmNum,
      TR::Node * &errorNode)
   {
   GPUResult result;

   static bool enableExceptionChecks = (feGetEnv("TR_disableGPUExceptionCheck") == NULL);
   TR::ILOpCode opcode = node->getOpCode();

   char name0[MAX_NAME], name1[MAX_NAME];
   bool isGenerated = false;
   bool printChildrenWithRefCount1 = true;

   if (node->isProfilingCode())
      {
      // Nothing to generate for profiling code, but we still need to visit the children
      // We can skip over the children with a reference count of one since they aren't used anywhere else.
      isGenerated = true;
      printChildrenWithRefCount1 = false;
      }

   if (node->getOpCodeValue() == TR::compressedRefs)
      {
      if (loop->isExprInvariant(node))
         return GPUSuccess; // ignore for now
      node = node->getFirstChild();
      }

   if (node->getOpCodeValue() == TR::treetop)
       node = node->getFirstChild();

   if (self()->comp()->isGPUCompilation() &&
       opcode.isLoadVarDirect() &&
       isThisPointer(node->getSymbolReference()))
       return GPUSuccess;   // will handle in the parent

   if (opcode.isLoadConst())
       if((node->getDataType() == TR::Address) && (node->getAddress() != 0))
          {
          traceMsg(self()->comp(), "Load Const with a non-zero address in node %p\n", node);
          return GPUInvalidProgram;
          }
       else
          return GPUSuccess;   // will handle in the parent

   if (node->getOpCodeValue() == TR::asynccheck)
       return GPUSuccess;

   if (!enableExceptionChecks &&
       (opcode.isNullCheck() || opcode.isBndCheck() || node->getOpCodeValue() == TR::DIVCHK))
       return GPUSuccess;


   if (node->getVisitCount() == visitCount)
      return GPUSuccess;

   node->setVisitCount(visitCount);

   if (opcode.isNullCheck())
      {

      TR::Node *refNode = node->getNullCheckReference();

      if (isSharedMemory(refNode, sharedMemory, self()->comp()))
         {
         // Shared Memory is always allocated
         ir.print("; DELETE NULLCHK [%p] since this reference [%p] is allocated in shared memory\n",
                  node, refNode);
         return GPUSuccess;
         }
      if (_gpuPostDominators && _gpuPostDominators->dominates(_gpuCurrentBlock, _gpuStartBlock))
         {
         TR::SymbolReference *symRef = refNode->getSymbolReference();
         if (symRef->getSymbol()->isParm())
            {
            int32_t argpos = symRef->getCPIndex();
            ir.print("; DELETE NULLCHK [%p] (ref[%p] is parm %d) since BB[%d] postdominates BB[%d]\n",
                     node, refNode, argpos, _gpuCurrentBlock->getNumber(), _gpuStartBlock->getNumber());
            _gpuNeedNullCheckArguments_vector |= (1L << (uint64_t)argpos);
            return GPUSuccess;
            }
         }

      result = self()->printNVVMIR(ir, refNode, loop, targetBlocks, visitCount, sharedMemory, nextParmNum, errorNode);
      if (result != GPUSuccess) return result;

      getNodeName(refNode, name0, self()->comp());
      const char *type0 = getTypeName(refNode->getDataType());

      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = icmp eq %s %s, null\n",
                   node->getLocalIndex(), type0, name0, name1);
      ir.print("  br i1 %%%d, label %%NullException, label %%nullchk_fallthru_%d, !prof !0\n",
                   node->getLocalIndex(), node->getLocalIndex());
      ir.print("nullchk_fallthru_%d:\n", node->getLocalIndex());

      _gpuHasNullCheck = true;
      isGenerated = true;
      }
   else if (opcode.isBndCheck())
      {
      bool isSMReference = false;
      int32_t smsize = -1;
      if (node->getChild(0)->getOpCodeValue() == TR::arraylength)
         {
         TR::Node *refNode = node->getChild(0)->getChild(0);
         if (isSharedMemory(refNode, sharedMemory, self()->comp()))
            {
            TR_SharedMemoryField field = sharedMemory->find(self()->comp(), refNode->getSymbolReference());
            smsize = field.getSize();
            TR_ASSERT(smsize > 0, "should be annotated as shared array with positive size");

            isSMReference = true;
            ir.print("; USE CONSTANT LENGTH %d for NULLCHK [%p] since this reference [%p] is allocated in shared memory\n",
                     smsize, node, refNode);
            }
         }

      if (!isSMReference)
         {
         result = self()->printNVVMIR(ir, node->getChild(0), loop, targetBlocks, visitCount, sharedMemory, nextParmNum, errorNode);
         if (result != GPUSuccess) return result;
         }
      result = self()->printNVVMIR(ir, node->getChild(1), loop, targetBlocks, visitCount, sharedMemory, nextParmNum, errorNode);
      if (result != GPUSuccess) return result;

      if (!isSMReference)
         {
         getNodeName(node->getChild(0), name0, self()->comp());
         }
      else
         {
         int32_t len = snprintf(name0, MAX_NAME, "%d", smsize);
         TR_ASSERT(len < MAX_NAME, "Node's name is too long\n");
         }

      getNodeName(node->getChild(1), name1, self()->comp());
      const char *type0 = getTypeName(node->getChild(0)->getDataType());

      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = icmp ule %s %s, %s\n",
                   node->getLocalIndex(), type0, name0, name1);
      ir.print("  br i1 %%%d, label %%BndException, label %%bndchk_fallthru_%d, !prof !0\n",
                   node->getLocalIndex(), node->getLocalIndex());
      ir.print("bndchk_fallthru_%d:\n", node->getLocalIndex());

      _gpuHasBndCheck = true;
      isGenerated = true;
      }
   else if (node->getOpCodeValue() == TR::DIVCHK)
      {
      TR::Node *idivNode = node->getChild(0);
      result = self()->printNVVMIR(ir, idivNode->getChild(0), loop, targetBlocks, visitCount, sharedMemory, nextParmNum, errorNode);
      if (result != GPUSuccess) return result;

      result = self()->printNVVMIR(ir, idivNode->getChild(1), loop, targetBlocks, visitCount, sharedMemory, nextParmNum, errorNode);
      if (result != GPUSuccess) return result;

      getNodeName(idivNode->getChild(1), name0, self()->comp());
      const char *type0 = getTypeName(idivNode->getChild(1)->getDataType());

      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = icmp eq %s %s, 0\n",
                   node->getLocalIndex(), type0, name0);
      ir.print("  br i1 %%%d, label %%DivException, label %%divchk_fallthru_%d, !prof !0\n",
                   node->getLocalIndex(), node->getLocalIndex());
      ir.print("divchk_fallthru_%d:\n", node->getLocalIndex());

      _gpuHasDivCheck = true;
      isGenerated = true;
      }

   // This symbol reference should become a parameter
   // children should be skipped (they are loop invariant)
   if (node->getOpCode().isLoadVar() &&
       _gpuSymbolMap[node->getSymbolReference()->getReferenceNumber()]._parmSlot != -1)
      {
      getParmName(_gpuSymbolMap[node->getSymbolReference()->getReferenceNumber()]._parmSlot, name0);

      node->setLocalIndex(_gpuNodeCount++);
      traceMsg(self()->comp(), "node %p assigned index %d\n", node, node->getLocalIndex());

      ir.print("  %%%d = %s %s* %s, align %d\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getDataType()),
                   name0,
                   node->getSize());
      return GPUSuccess;
      }

   //Don't run printNVVMIR on a children node if:
   //(they are the child of a profiling call) AND ((have a reference count less then two) OR (is a loadConst node))
   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      if ((child->getReferenceCount() >= 2 && !child->getOpCode().isLoadConst()) || printChildrenWithRefCount1)
         {
         result = self()->printNVVMIR(ir, child, loop, targetBlocks, visitCount, sharedMemory, nextParmNum, errorNode);
         if (result != GPUSuccess)
            return result;
         }
      }

   if (isGenerated)
      {
      return GPUSuccess;
      }

   node->setLocalIndex(_gpuNodeCount++);
   traceMsg(self()->comp(), "node %p assigned index %d\n", node, node->getLocalIndex());


   if (node->getOpCodeValue() == TR::PassThrough)
      {
      node->setLocalIndex(_gpuNodeCount--);
      return GPUSuccess;
      }
   else if (node->getOpCodeValue() == TR::BBStart)
      {
      node->setLocalIndex(_gpuNodeCount--);
      _gpuCurrentBlock = node->getBlock();
      if (targetBlocks->get(_gpuCurrentBlock->getNumber()))
          ir.print("block_%d:\n", _gpuCurrentBlock->getNumber());
      }
   // if block has a label previous block has to end with a branch
   else if (node->getOpCodeValue() == TR::BBEnd &&
       !_gpuCurrentBlock->endsInBranch() &&
       !_gpuCurrentBlock->getLastRealTreeTop()->getNode()->getOpCode().isReturn() &&
       !_gpuCurrentBlock->getLastRealTreeTop()->getNode()->getOpCode().isGoto() &&
       !_gpuCurrentBlock->getLastRealTreeTop()->getNode()->getOpCode().isSwitch() &&
       _gpuCurrentBlock->getNextBlock() &&
       targetBlocks->get(_gpuCurrentBlock->getNextBlock()->getNumber()))
      {
      node->setLocalIndex(_gpuNodeCount--);
      ir.print("  br label %%block_%d\n",
                   _gpuCurrentBlock->getNextBlock()->getNumber());
      }
   else if (node->getOpCodeValue() == TR::BBEnd)
      {
      node->setLocalIndex(_gpuNodeCount--);
      }
   else if (node->getOpCode().isReturn())
      {
      node->setLocalIndex(_gpuNodeCount--);

      if (node->getNumChildren() == 0)
         {
         ir.print("  %s void\n",
                      getOpCodeName(node->getOpCodeValue()));
         }
      else
         {
         TR_ASSERT(node->getNumChildren() == 1, "Unsupported return\n");
         getNodeName(node->getChild(0), name0, self()->comp());
         ir.print("  %s %s %s\n",
                      getOpCodeName(node->getOpCodeValue()),
                      getTypeName(node->getDataType()),
                      name0);
         }
      }
   else if (node->getOpCode().isStoreIndirect()) // TODO: handle statics
      {
      TR::Node *firstChild = node->getChild(0);
      TR::Node *secondChild = node->getChild(1);
      getNodeName(firstChild, name0, self()->comp());
      getNodeName(secondChild, name1, self()->comp());

         {
         _gpuNodeCount--;
         ir.print("  %%%d = bitcast %s %s to %s %s*\n",
                      _gpuNodeCount,
                      getTypeName(firstChild->getDataType()), name0,
                      getTypeName(secondChild->getDataType()),
                      firstChild->chkSharedMemory() ? "addrspace(3)" : "");

         ir.print("  %s %s %s, %s %s * %%%d, align %d\n",
                      getOpCodeName(node->getOpCodeValue()),
                      getTypeName(secondChild->getDataType()),
                      name1,
                      getTypeName(secondChild->getDataType()),
                      firstChild->chkSharedMemory() ? "addrspace(3)" : "",
                      _gpuNodeCount,
                      secondChild->getSize());
         _gpuNodeCount++;
         }
      }
   else if (node->getOpCode().isLoadIndirect()) // TODO: handle statics
      {
      TR::Node *firstChild = node->getChild(0);

      getNodeName(firstChild, name0, self()->comp());

      if (node->getSymbolReference()->getCPIndex() != -1) // field of some object
         {
         // TODO: check that field is an array!
         TR_ASSERT(firstChild->getOpCode().isLoadDirect() &&
                   isThisPointer(firstChild->getSymbolReference()),
                   "can only access a field of this object\n");

         // TODO: handle duplicate names from different classes
         TR_SharedMemoryField field = sharedMemory->find(self()->comp(), node->getSymbolReference());
         TR_ASSERT(field.getSize() >= 0, "field was not found in this object\n");

         if (field.getSize() > 0)
            {
            ir.print("  %%%d = bitcast [%d x %s] addrspace(3)* @%.*s to i8*\n",
                      node->getLocalIndex(),
                      field.getSize(),
                      getTypeNameFromSignature(field.getFieldSig(), field.getFieldSigLength()),
                      field.getFieldNameLength(), field.getFieldName());

            node->setSharedMemory(true);
            }
         else
            {
            int32_t parmNum = field.getParmNum();
            if (parmNum == -1)
               {
               sharedMemory->setParmNum(self()->comp(), node->getSymbolReference(), nextParmNum);
               parmNum = nextParmNum++;
               }
            ir.print("  %%%d = %s %s* %%p%d.addr, align %d\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getDataType()),
                   parmNum,
                   node->getSize());
            }
         }
      else
         {
         // assume SM35 or more
         static bool disableReadOnlyCacheArray = (feGetEnv("TR_disableGPUReadOnlyCacheArray") != NULL);
         bool isReadOnlyArray = false;
         if (node->getSymbolReference()->getSymbol()->isArrayShadowSymbol() &&
             // I disabled to generate ld.global.nc for read-only address array
             // I do not know an intrinsic function name for ld.global.nc of address
             node->getDataType() != TR::Address &&
             !disableReadOnlyCacheArray &&
             _gpuCanUseReadOnlyCache)
            {
            TR::Node *addrNode = node->getFirstChild();
            if (addrNode->getOpCodeValue() == TR::aiadd || addrNode->getOpCodeValue() == TR::aladd)
               {
               addrNode = addrNode->getFirstChild();
               }
            if ((addrNode->getOpCodeValue() == TR::aload) || (addrNode->getOpCodeValue() == TR::aloadi))
               {
               TR::SymbolReference *symRef = addrNode->getSymbolReference();
               int32_t symRefIndex = symRef->getReferenceNumber();
               CS2::ArrayOf<gpuMapElement, TR::Allocator> &gpuSymbolMap = self()->comp()->cg()->_gpuSymbolMap;

               int32_t nc = symRefIndex;
               TR::SymbolReference *hostSymRef = gpuSymbolMap[nc]._hostSymRef;
               int32_t parmSlot = gpuSymbolMap[nc]._parmSlot;

               if (!hostSymRef || parmSlot == -1)
                  {
                  TR::Node *tempNode = gpuSymbolMap[nc]._node;
                  if (tempNode && (tempNode->getOpCodeValue() == TR::astore) && (tempNode->getFirstChild()->getOpCodeValue() == TR::aloadi))
                     {
                     TR::Node *parmNode = tempNode->getFirstChild();
                     nc = parmNode->getSymbolReference()->getReferenceNumber();

                     hostSymRef = gpuSymbolMap[nc]._hostSymRef;
                     parmSlot = gpuSymbolMap[nc]._parmSlot;
                     }
                  }
               else if (hostSymRef->getReferenceNumber() != symRefIndex)
                  {
                  hostSymRef = NULL;
                  }

               if (hostSymRef && (parmSlot != -1) &&
                   (gpuSymbolMap[nc]._accessKind & TR::CodeGenerator::ReadWriteAccesses) == TR::CodeGenerator::ReadAccess)
                  {
                  isReadOnlyArray = true;
                  }
               }
            }

         ir.print("  %%%d = bitcast %s %s to %s %s*\n",
                      _gpuNodeCount-1,
                      getTypeName(firstChild->getDataType()),
                      name0,
                      getTypeName(node->getDataType()),
                      firstChild->chkSharedMemory() ? "addrspace(3)" : isReadOnlyArray ? "addrspace(1)" : "");

         node->setLocalIndex(_gpuNodeCount++);
         traceMsg(self()->comp(), "node %p assigned index %d\n", node, node->getLocalIndex());

         //NVVM 1.3 onward uses a two parameter version of ldg
         if (isReadOnlyArray)
            {
            if (_gpuUseOldLdgCalls)
               {
               ir.print("  %%%d = tail call %s @llvm.nvvm.ldg.global.%s.%s.p1%s(%s addrspace(1)* %%%d), !align !1%d\n",
                        node->getLocalIndex(),
                        getTypeName(node->getDataType()),
                        (node->getDataType() >= TR::Float) ? "f" : "i",
                        getVarTypeName(node->getDataType()),
                        getVarTypeName(node->getDataType()),
                        getTypeName(node->getDataType()),
                        _gpuNodeCount-2,
                        node->getSize());
               }
            else
               {
               ir.print("  %%%d = tail call %s @llvm.nvvm.ldg.global.%s.%s.p1%s(%s addrspace(1)* %%%d, i32 %d)\n",
                        node->getLocalIndex(),
                        getTypeName(node->getDataType()),
                        (node->getDataType() >= TR::Float) ? "f" : "i",
                        getVarTypeName(node->getDataType()),
                        getVarTypeName(node->getDataType()),
                        getTypeName(node->getDataType()),
                        _gpuNodeCount-2,
                        node->getSize());
               }
            }
         else
            // e.g. %32 = load i32 addrspace(4) * %31, align 4
            ir.print("  %%%d = %s %s %s * %%%d, align %d\n",
                     node->getLocalIndex(),
                     getOpCodeName(node->getOpCodeValue()),
                     getTypeName(node->getDataType()),
                     firstChild->chkSharedMemory() ? "addrspace(3)" : "",
                     _gpuNodeCount-2,
                     node->getSize());
         }
      }
   else if (node->getOpCode().isCall() &&
            ((TR::MethodSymbol*)node->getSymbolReference()->getSymbol())->getRecognizedMethod() != TR::unknownMethod &&
            self()->handleRecognizedMethod(node, ir, self()->comp()))
      {
      }
   else if (node->getOpCodeValue() == TR::arraycopy)
      {
      self()->printArrayCopyNVVMIR(node, ir, self()->comp());
      }
   else if (node->getOpCode().isCall())
      {
      traceMsg(self()->comp(), "unrecognized call %p\n", node);
      return GPUInvalidProgram;
      }
   else if (node->getOpCode().isStoreDirect() &&
            node->getSymbolReference()->getSymbol()->getRecognizedField() != TR::Symbol::UnknownField)
      {
      switch (node->getSymbolReference()->getSymbol()->getRecognizedField())
         {
         case TR::Symbol::Com_ibm_gpu_Kernel_syncThreads:
             ir.print("  call void @llvm.nvvm.barrier0()\n");
             break;
         default:
             break;
         }
      node->setLocalIndex(_gpuNodeCount--);
      }
   else if (node->getOpCode().isLoadVarDirect() &&
            node->getSymbolReference()->getSymbol()->getRecognizedField() != TR::Symbol::UnknownField &&
            self()->handleRecognizedField(node, ir))
      {
      }
   else if (node->getOpCode().isLoadVarDirect())
      {
      if (!node->getSymbol()->isAutoOrParm())
         {
         traceMsg(self()->comp(), "unexpected symbol in node %p\n");
         return GPUInvalidProgram;
         }

      getAutoOrParmName(node->getSymbol(), name0);

      ir.print("  %%%d = %s %s* %s, align %d\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getDataType()),
                   name0,
                   node->getSize());
      }
   else if (node->getOpCode().isStoreDirect())
      {
      if (!node->getSymbol()->isAutoOrParm())
         {
         traceMsg(self()->comp(), "unexpected symbol in node %p\n");
         return GPUInvalidProgram;
         }

      getNodeName(node->getChild(0), name0, self()->comp());
      getAutoOrParmName(node->getSymbol(), name1);

      ir.print("  %s %s %s, %s* %s, align %d\n",
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getChild(0)->getDataType()),
                   name0,
                   getTypeName(node->getDataType()),
                   name1,
                   node->getChild(0)->getSize());

      node->setLocalIndex(_gpuNodeCount--);
      }
   else if (node->getOpCode().isArrayRef())
      {
      getNodeName(node->getChild(0), name0, self()->comp());
      getNodeName(node->getChild(1), name1, self()->comp());

      ir.print("  %%%d = %s inbounds %s %s, %s %s\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getChild(0)->getDataType()),
                   name0,
                   getTypeName(node->getChild(1)->getDataType()), name1);

      if (node->getChild(0)->chkSharedMemory())
          node->setSharedMemory(true);
      }
   else if (node->getOpCodeValue() == TR::arraylength)
      {
      // assume SM35 or more
      static bool disableReadOnlyCacheObjHdr = (feGetEnv("TR_disableGPUReadOnlyCacheObjHdr") != NULL);
      getNodeName(node->getChild(0), name0, self()->comp());

      ir.print("  %%%d = getelementptr inbounds i8* %s, i32 %d\n",
                   node->getLocalIndex(),
                   name0,
                   self()->objectLengthOffset());

      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = bitcast i8* %%%d to i32 %s*\n",
               node->getLocalIndex(),
               node->getLocalIndex() - 1,
               (_gpuCanUseReadOnlyCache && !disableReadOnlyCacheObjHdr) ? "addrspace(1)" : "");

      node->setLocalIndex(_gpuNodeCount++);

      //NVVM 1.3 onward uses a two parameter version of ldg
      if (_gpuCanUseReadOnlyCache && !disableReadOnlyCacheObjHdr)
         {
         if (_gpuUseOldLdgCalls)
            {
            ir.print("  %%%d = tail call i32 @llvm.nvvm.ldg.global.i.i32.p1i32(i32 addrspace(1)* %%%d), !align !14\n",
                     node->getLocalIndex(),
                     node->getLocalIndex() - 1);
            }
         else
            {
            ir.print("  %%%d = tail call i32 @llvm.nvvm.ldg.global.i.i32.p1i32(i32 addrspace(1)* %%%d, i32 4)\n",
                     node->getLocalIndex(),
                     node->getLocalIndex() - 1);
            }
         }
      else
         ir.print("  %%%d = load i32* %%%d, align 4\n",
                  node->getLocalIndex(),
                  node->getLocalIndex() - 1);
      }
   // Binary Operations
   else if ((node->getOpCodeValue() == TR::lshl ||
             node->getOpCodeValue() == TR::lshr) &&
            node->getChild(1)->getDataType() == TR::Int32)
      {
      getNodeName(node->getChild(0), name0, self()->comp());
      getNodeName(node->getChild(1), name1, self()->comp());

      ir.print("  %%%d = sext i32 %s to i64\n",
               node->getLocalIndex(),
               name1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = %s %s %s, %%%d\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getDataType()),
                   name0, _gpuNodeCount-2);

      }
   else if (node->getOpCodeValue() == TR::imulh || node->getOpCodeValue() == TR::iumulh ||
            node->getOpCodeValue() == TR::lmulh || node->getOpCodeValue() == TR::lumulh)
      {
      getNodeName(node->getChild(0), name0, self()->comp());
      getNodeName(node->getChild(1), name1, self()->comp());

      bool isLongMul = node->getOpCodeValue() == TR::lmulh || node->getOpCodeValue() == TR::lumulh;
      bool isSignedMul = node->getOpCodeValue() == TR::imulh || node->getOpCodeValue() == TR::lmulh;

      bool extendChild0 = isLongMul || (node->getChild(0)->getDataType() != TR::Int64);
      bool extendChild1 = isLongMul || (node->getChild(1)->getDataType() != TR::Int64);

      if (extendChild0)
         {
         ir.print("  %%%d = %s %s %s to %s\n",
                      node->getLocalIndex(),
                      isSignedMul ? "sext" : "zext",
                      getTypeName(node->getChild(0)->getDataType()),
                      name0,
                      isLongMul ? "i128" : "i64");
         node->setLocalIndex(_gpuNodeCount++);
         }
      else
         {
         ir.print("  %%%d = lshr i64 %s, 0\n",
                      node->getLocalIndex(),
                      name0);
         node->setLocalIndex(_gpuNodeCount++);
         }

      if(extendChild1)
         {
         ir.print("  %%%d = %s %s %s to %s\n",
                      node->getLocalIndex(),
                      isSignedMul ? "sext" : "zext",
                      getTypeName(node->getChild(1)->getDataType()),
                      name1,
                      isLongMul ? "i128" : "i64");
         node->setLocalIndex(_gpuNodeCount++);
         }
      else
         {
         ir.print("  %%%d = lshr i64 %s, 0\n",
                      node->getLocalIndex(),
                      name1);
         node->setLocalIndex(_gpuNodeCount++);
         }

      ir.print("  %%%d = mul %s %%%d, %%%d\n",
                   node->getLocalIndex(),
                   isLongMul ? "i128" : "i64",
                   node->getLocalIndex()-2,
                   node->getLocalIndex()-1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = lshr %s %%%d, %s\n",
                   node->getLocalIndex(),
                   isLongMul ? "i128" : "i64",
                   node->getLocalIndex()-1,
                   isLongMul ? "64" : "32");
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = trunc %s %%%d to %s\n",
                   node->getLocalIndex(),
                   isLongMul ? "i128" : "i64",
                   node->getLocalIndex()-1,
                   isLongMul ? "i64" : "i32");
      }
   else if (node->getOpCodeValue() == TR::bneg || node->getOpCodeValue() == TR::sneg ||
            node->getOpCodeValue() == TR::ineg || node->getOpCodeValue() == TR::lneg ||
            node->getOpCodeValue() == TR::iuneg || node->getOpCodeValue() == TR::luneg ||
            node->getOpCodeValue() == TR::fneg || node->getOpCodeValue() == TR::dneg)
      {
      getNodeName(node->getChild(0), name0, self()->comp());

      bool isFloatDouble = node->getOpCodeValue() == TR::fneg || node->getOpCodeValue() == TR::dneg;

      ir.print("  %%%d = %s %s %s, %s\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getDataType()),
                   isFloatDouble ? "0.0" : "0",
                   name0);
      }
   else if (node->getOpCodeValue() == TR::iabs || node->getOpCodeValue() == TR::labs)
      {
      getNodeName(node->getChild(0), name0, self()->comp());

      bool isInt = node->getOpCodeValue() == TR::iabs;

      ir.print("  %%%d = ashr %s %s, %s\n",
                   node->getLocalIndex(),
                   getTypeName(node->getDataType()),
                   name0,
                   isInt ? "31" : "63");
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = xor %s %s, %%%d\n",
                   node->getLocalIndex(),
                   getTypeName(node->getDataType()),
                   name0,
                   node->getLocalIndex()-1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = sub %s %%%d, %%%d\n",
                   node->getLocalIndex(),
                   getTypeName(node->getDataType()),
                   node->getLocalIndex()-1,
                   node->getLocalIndex()-2);
      }
   else if (node->getOpCodeValue() == TR::irol || node->getOpCodeValue() == TR::lrol)
      {
      getNodeName(node->getChild(0), name0, self()->comp());
      getNodeName(node->getChild(1), name1, self()->comp());

      bool isInt = node->getOpCodeValue() == TR::irol;

      ir.print("  %%%d = shl %s %s, %s\n",
                   node->getLocalIndex(),
                   getTypeName(node->getDataType()),
                   name0, name1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = sub %s %s, %s\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(1)->getDataType()),
                   isInt ? "32" : "64",
                   name1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = and %s %%%d, %s\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(1)->getDataType()),
                   node->getLocalIndex()-1,
                   isInt ? "31" : "63");
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = lshr %s %s, %%%d\n",
                   node->getLocalIndex(),
                   getTypeName(node->getDataType()),
                   name0,
                   node->getLocalIndex()-1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = or %s %%%d, %%%d\n",
                   node->getLocalIndex(),
                   getTypeName(node->getDataType()),
                   node->getLocalIndex()-4,
                   node->getLocalIndex()-1);
      }
   else if (node->getOpCodeValue() == TR::ibits2f || node->getOpCodeValue() == TR::fbits2i ||
            node->getOpCodeValue() == TR::lbits2d || node->getOpCodeValue() == TR::dbits2l)
      {
      getNodeName(node->getChild(0), name0, self()->comp());

      ir.print("  %%%d = %s %s %s to %s\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getChild(0)->getDataType()),
                   name0,
                   getTypeName(node->getDataType()));
      }
   else if (node->getOpCode().isArithmetic() &&
            node->getNumChildren() == 2 &&
            getOpCodeName(node->getOpCodeValue()))   // supported binary opcode
      {
      getNodeName(node->getChild(0), name0, self()->comp());
      getNodeName(node->getChild(1), name1, self()->comp());

      ir.print("  %%%d = %s %s %s, %s\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getDataType()),
                   name0, name1);
      }
   else if (node->getOpCode().isConversion() &&
            getOpCodeName(node->getOpCodeValue()))
      {
      getNodeName(node->getChild(0), name0, self()->comp());

      ir.print("  %%%d = %s %s %s to %s\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getChild(0)->getDataType()),
                   name0,
                   getTypeName(node->getDataType()));
      }
   else if (node->getOpCode().isIf())
      {
      getNodeName(node->getChild(0), name0, self()->comp());
      getNodeName(node->getChild(1), name1, self()->comp());
      const char *type0 = getTypeName(node->getChild(0)->getDataType());

      const char *opcode = getOpCodeName(node->getOpCodeValue());

      ir.print("  %%%d = %s %s %s, %s\n",
                   node->getLocalIndex(), opcode, type0, name0, name1);
      ir.print("  br i1 %%%d, label %%block_%d, label %%block_%d\n",
                   node->getLocalIndex(),
                   node->getBranchDestination()->getNode()->getBlock()->getNumber(),
                   _gpuCurrentBlock->getNextBlock()->getNumber());
      }
   else if (node->getOpCodeValue() == TR::Goto)
      {
      ir.print("  %s label %%block_%d\n",
                   getOpCodeName(node->getOpCodeValue()),
                   node->getBranchDestination()->getNode()->getBlock()->getNumber());
      node->setLocalIndex(_gpuNodeCount--);
      }
   else if (node->getOpCodeValue() == TR::lookup)
      {
      getNodeName(node->getChild(0), name0, self()->comp());

      ir.print("  %s %s %s, label %%block_%d [ ",
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getChild(0)->getDataType()),
                   name0,
                   node->getChild(1)->getBranchDestination()->getNode()->getBlock()->getNumber()
                   );
      for(int i=2; i < node->getNumChildren(); ++i)
         {
         ir.print("%s %d, label %%block_%d ",
                      getTypeName(node->getChild(0)->getDataType()),
                      node->getChild(i)->getCaseConstant(),
                      node->getChild(i)->getBranchDestination()->getNode()->getBlock()->getNumber());
         }
      ir.print("]\n");
      node->setLocalIndex(_gpuNodeCount--);
      }
   else if (node->getOpCodeValue() == TR::table)
         {
         getNodeName(node->getChild(0), name0, self()->comp());

         ir.print("  %s %s %s, label %%block_%d [ ",
                      getOpCodeName(node->getOpCodeValue()),
                      getTypeName(node->getChild(0)->getDataType()),
                      name0,
                      node->getChild(1)->getBranchDestination()->getNode()->getBlock()->getNumber()
                      );
         for(int i=2; i < node->getNumChildren(); ++i)
            {
            ir.print("%s %d, label %%block_%d ",
                         getTypeName(node->getChild(0)->getDataType()),
                         i-2,
                         node->getChild(i)->getBranchDestination()->getNode()->getBlock()->getNumber());
            }
         ir.print("]\n");
         node->setLocalIndex(_gpuNodeCount--);
         }
   else if (node->getOpCode().isBooleanCompare()) //Needs to be after "isIf()" check
      {
      getNodeName(node->getChild(0), name0, self()->comp());
      getNodeName(node->getChild(1), name1, self()->comp());
      const char *type0 = getTypeName(node->getChild(0)->getDataType());

      const char *opcode = getOpCodeName(node->getOpCodeValue());

      ir.print("  %%%d = %s %s %s, %s\n",
                   node->getLocalIndex(), opcode, type0, name0, name1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = zext i1 %%%d to i32\n",
                   node->getLocalIndex(),
                   node->getLocalIndex()-1);
      }
   else if (node->getOpCodeValue() == TR::treetop || node->getOpCodeValue() == TR::Case)
      {
      node->setLocalIndex(_gpuNodeCount--);
      }
   else if (getOpCodeName(node->getOpCodeValue()) &&
            strcmp(getOpCodeName(node->getOpCodeValue()), "INVALID") == 0)
      {
      node->setLocalIndex(_gpuNodeCount--);
      traceMsg(self()->comp(), "INVALID operation required by node %p\n", node);
      return GPUInvalidProgram;
      }
   else
      {
      node->setLocalIndex(_gpuNodeCount--);
      traceMsg(self()->comp(), "node %p assigned index %d\n", node, node->getLocalIndex());
      traceMsg(self()->comp(), "unsupported opcode (%s) on line %d %p\n", node->getOpCode().getName(), self()->comp()->getLineNumber(node), node);
      return GPUInvalidProgram;
      }

   return GPUSuccess;
   }


void traceNVVMIR(TR::Compilation *comp, char *buffer)
   {
   traceMsg(comp, "NVVM IR:\n");
   char msg[256];
   char *cs = buffer;
   int line = 1;
   while (*cs != '\0')
      {
      char *ce = cs;
      while (*ce != '\n' && *ce != '\0')
         {
         ce++;
         }
      ce++;
      int len = (ce - cs) < 255 ? (ce - cs) : 255;
      memcpy(msg, cs, len);
      msg[len] = '\0';
      traceMsg(comp, "%6d: %s", line++, msg);
      if (*(ce - 1) == '\0')
         {
         ce--;
         }
      cs = ce;
      }
   traceMsg(comp, "\n");
   }


void
J9::CodeGenerator::findExtraParms(
      TR::Node *node,
      int32_t &numExtraParms,
      TR_SharedMemoryAnnotations *sharedMemory,
      vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   if (node->getOpCode().isLoadIndirect() &&
       _gpuSymbolMap[node->getSymbolReference()->getReferenceNumber()]._parmSlot == -1)
      {
      TR::Node *firstChild = node->getChild(0);
      if (node->getSymbolReference()->getCPIndex() != -1) // field of some object
         {
         // TODO: check that field is an array!
         TR_ASSERT(firstChild->getOpCode().isLoadDirect() &&
                   isThisPointer(firstChild->getSymbolReference()),
                   "can only access a field of this object\n");

         // TODO: handle duplicate names from different classes
         TR_SharedMemoryField field = sharedMemory->find(TR::comp(), node->getSymbolReference());

         if (field.getSize() == 0)
            numExtraParms++;
         }
      }

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      self()->findExtraParms(child, numExtraParms, sharedMemory, visitCount);
      }
   }


void
J9::CodeGenerator::dumpInvariant(
      CS2::ArrayOf<gpuParameter, TR::Allocator>::Cursor pit,
      NVVMIRBuffer &ir,
      bool isbufferalign)
   {
   return;

   for (pit.SetToFirst(); pit.Valid(); pit.SetToNext())
      {
      TR::Symbol *sym = pit->_hostSymRef->getSymbol();
      char parmName[MAX_NAME+2];
      getParmName(pit->_parmSlot, parmName, false);
      if (sym->getDataType() == TR::Address)
         {
         if (isbufferalign)
            strcat(parmName, ".t");
         ir.print("  call void @llvm.invariant.end({}* %%inv_%s_header, i64 %d, i8* %s)\n",
                                         &parmName[1], self()->objectHeaderInvariant(), parmName);
         }
      }
   }

#ifdef ENABLE_GPU
bool calculateComputeCapability(int tracing, short* computeMajor, short* computeMinor, int deviceId);
bool getNvvmVersion(int tracing, int* majorVersion, int* minorVersion);
#endif

TR::CodeGenerator::GPUResult
J9::CodeGenerator::dumpNVVMIR(
      TR::TreeTop *firstTreeTop,
      TR::TreeTop *lastTreeTop,
      TR_RegionStructure *loop,
      SharedSparseBitVector *blocksInLoop,
      ListBase<TR::AutomaticSymbol> *autos,
      ListBase<TR::ParameterSymbol> *parms,
      bool staticMethod,
      char * &nvvmIR,
      TR::Node * &errorNode,
      int gpuPtxCount,
      bool* hasExceptionChecks)
   {
   static bool isbufferalign = feGetEnv("TR_disableGPUBufferAlign") ? false : true;
   NVVMIRBuffer ir(self()->comp()->trMemory());
   GPUResult result;
   short computeMajor, computeMinor, computeCapability;
   int nvvmMajorVersion = 0;
   int nvvmMinorVersion = 0;

   _gpuHasNullCheck = false;
   _gpuHasBndCheck = false;
   _gpuHasDivCheck = false;
   _gpuNodeCount = 0;
   _gpuReturnType = TR::NoType;
   _gpuPostDominators = NULL;
   _gpuStartBlock = NULL;
   _gpuNeedNullCheckArguments_vector = 0;
   _gpuCanUseReadOnlyCache = false;
   _gpuUseOldLdgCalls = false;

#ifdef ENABLE_GPU
   if (!calculateComputeCapability(/*tracing*/0, &computeMajor, &computeMinor, /*deviceId*/0))
      {
      traceMsg(self()->comp(), "calculateComputeCapability was unsuccessful.\n");
      return GPUHelperError;
      }
   computeCapability = 100*computeMajor + computeMinor; //combines Major and Minor versions into a single number.

   if (computeCapability >= 305) //If compute capability is 3.5 or higher
      _gpuCanUseReadOnlyCache = true; //then the GPU is capable of using read only cache

   if (!getNvvmVersion(/*tracing*/0, &nvvmMajorVersion, &nvvmMinorVersion))
      {
      traceMsg(self()->comp(), "getNvvmVersion was unsuccessful.\n");
      return GPUHelperError;
      }

   /*
    * NVVM 1.3 updates LLVM support to LLVM 3.8. From LLVM 3.6 onward, ldg was changed to make alignment an explicit
    * parameter instead of as metadata. As a result, NVVM 1.2 and before uses a one parameter version of ldg while
    * NVVM 1.3 and onward uses a two parameter version.
    */
   if (nvvmMajorVersion == 1 && nvvmMinorVersion <= 2)
      {
      _gpuUseOldLdgCalls = true;
      }
#endif

   TR::CFG *cfg = self()->comp()->getFlowGraph();
   TR_BitVector targetBlocks(cfg->getNumberOfNodes(), self()->comp()->trMemory(), stackAlloc, growable);

   static bool enableExceptionCheckElimination = (feGetEnv("TR_enableGPUExceptionCheckElimination") != NULL);
   if (enableExceptionCheckElimination)
      {
      _gpuPostDominators = new (self()->comp()->trStackMemory()) TR_Dominators(self()->comp(), true);
      }
   _gpuStartBlock = toBlock(cfg->getStart());


   TR_SharedMemoryAnnotations sharedMemory(self()->comp());
   int32_t numExtraParms = 0;

   // First pass through the trees
   vcount_t visitCount = self()->comp()->incVisitCount();

   int32_t currentBlock = 0;
   int32_t firstBlock = 0;
   for (TR::TreeTop * tree = firstTreeTop; tree != lastTreeTop; tree = tree->getNextTreeTop())
      {
      if (tree->getNode()->getOpCodeValue() == TR::BBStart)
          currentBlock = tree->getNode()->getBlock()->getNumber();

      if (blocksInLoop && !blocksInLoop->ValueAt(currentBlock))
          continue;

      if (firstBlock == 0)
          firstBlock = currentBlock;

      TR::Node *node = tree->getNode();
      if (node->getOpCode().isBranch())
         {
         TR_ASSERT(node->getBranchDestination()->getNode()->getOpCodeValue() == TR::BBStart, "Attempted to get Block number of a non-BBStart node.");
         targetBlocks.set(node->getBranchDestination()->getNode()->getBlock()->getNumber());

         if (tree->getNextTreeTop() &&
             tree->getNextTreeTop()->getNextTreeTop())
            {
            node = tree->getNextTreeTop()->getNextTreeTop()->getNode();
            TR_ASSERT(node->getOpCodeValue() == TR::BBStart, "Attempted to get Block number of a non-BBStart node.");
            targetBlocks.set(node->getBlock()->getNumber());
            }
         }
      else if (node->getOpCode().isSwitch())
         {
         for (int childIndex = 0; childIndex < node->getNumChildren(); ++childIndex)
            {
            if (node->getChild(childIndex)->getOpCode().isBranch())
               {
               TR_ASSERT(node->getChild(childIndex)->getBranchDestination()->getNode()->getOpCodeValue() == TR::BBStart, "Attempted to get Block number of a non-BBStart node.");
               targetBlocks.set(node->getChild(childIndex)->getBranchDestination()->getNode()->getBlock()->getNumber());
               }
            }
         }
      else if (node->getOpCode().isReturn())
         _gpuReturnType = node->getDataType().getDataType();

      //findExtraParms(node, numExtraParms, &sharedMemory, visitCount);
      }

   traceMsg(self()->comp(), "extra parameters = %d\n", numExtraParms);
   ir.print("target triple = \"nvptx64-unknown-cuda\"\n");
   ir.print("target datalayout = \"e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v16:16:16-v32:32:32-v64:64:64-v128:128:128-n16:32:64\"\n\n");  // TODO: 32-bit


   // TODO: alignment, arraylength !!!
   for(auto lit = sharedMemory.getSharedMemoryFields().begin(); lit != sharedMemory.getSharedMemoryFields().end(); ++lit)
       {
       if ((*lit).getSize() > 0)
          ir.print("@%.*s = internal addrspace(3) global [%d x %s] zeroinitializer, align 8\n",
        		  (*lit).getFieldNameLength(), (*lit).getFieldName(), (*lit).getSize(),
                     getTypeNameFromSignature((*lit).getFieldSig(), (*lit).getFieldSigLength()));
       }

   static bool disableReadOnlyCacheArray = (feGetEnv("TR_disableGPUReadOnlyCacheArray") != NULL);
   static bool disableReadOnlyCacheObjHdr = (feGetEnv("TR_disableGPUReadOnlyCacheObjHdr") != NULL);

   //ir.print("@_ExceptionKind = addrspace(1) global [1 x i32 0, align 4\n");
   ir.print("@_ExceptionKind = addrspace(1) global [1 x i32] zeroinitializer, align 4\n");

   //NVVM 1.3 onward uses a two parameter version of ldg
   if (_gpuCanUseReadOnlyCache && (!disableReadOnlyCacheArray || !disableReadOnlyCacheObjHdr))
      {
      if (_gpuUseOldLdgCalls)
         {
         ir.print("declare i8  @llvm.nvvm.ldg.global.i.i8.p1i8(i8 addrspace(1)* %%ptr)\n");
         ir.print("declare i16 @llvm.nvvm.ldg.global.i.i16.p1i16(i16 addrspace(1)* %%ptr)\n");
         ir.print("declare i32 @llvm.nvvm.ldg.global.i.i32.p1i32(i32 addrspace(1)* %%ptr)\n");
         ir.print("declare i64 @llvm.nvvm.ldg.global.i.i64.p1i64(i64 addrspace(1)* %%ptr)\n");
         ir.print("declare float  @llvm.nvvm.ldg.global.f.f32.p1f32(float addrspace(1)* %%ptr)\n");
         ir.print("declare double @llvm.nvvm.ldg.global.f.f64.p1f64(double addrspace(1)* %%ptr)\n");
         ir.print("declare i8*    @llvm.nvvm.ldg.global.p.p64.p1p64(i8* addrspace(1)* %%ptr)\n");
         }
      else
         {
         ir.print("declare i8  @llvm.nvvm.ldg.global.i.i8.p1i8(i8 addrspace(1)* %%ptr, i32 %%align)\n");
         ir.print("declare i16 @llvm.nvvm.ldg.global.i.i16.p1i16(i16 addrspace(1)* %%ptr, i32 %%align)\n");
         ir.print("declare i32 @llvm.nvvm.ldg.global.i.i32.p1i32(i32 addrspace(1)* %%ptr, i32 %%align)\n");
         ir.print("declare i64 @llvm.nvvm.ldg.global.i.i64.p1i64(i64 addrspace(1)* %%ptr, i32 %%align)\n");
         ir.print("declare float  @llvm.nvvm.ldg.global.f.f32.p1f32(float addrspace(1)* %%ptr, i32 %%align)\n");
         ir.print("declare double @llvm.nvvm.ldg.global.f.f64.p1f64(double addrspace(1)* %%ptr, i32 %%align)\n");
         ir.print("declare i8*    @llvm.nvvm.ldg.global.p.p64.p1p64(i8* addrspace(1)* %%ptr, i32 %%align)\n");
         }
      }

   ir.print("declare {}* @llvm.invariant.start(i64 %%size, i8* nocapture %%ptr)\n");
   ir.print("declare void @llvm.invariant.end({}* %%start, i64 %%size, i8* nocapture %%ptr)\n");

   ir.print("\ndefine %s @test%d(", getTypeName(_gpuReturnType), gpuPtxCount);

   CS2::ArrayOf<gpuParameter, TR::Allocator> gpuParameterMap(TR::comp()->allocator());
   CS2::ArrayOf<TR::CodeGenerator::gpuMapElement, TR::Allocator>::Cursor ait(_gpuSymbolMap);

   for (ait.SetToFirst(); ait.Valid(); ait.SetToNext())
      {
      if (!ait->_hostSymRef) continue;
      traceMsg(TR::comp(), "hostSymRef #%d parmSlot %d\n", (int)ait, ait->_parmSlot);

      if (ait->_parmSlot != -1)
         {
         gpuParameter parm (ait->_hostSymRef, ait->_parmSlot);
         gpuParameterMap[ait->_parmSlot] = parm;
         }
      }


   TR::ResolvedMethodSymbol *method = self()->comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol> pi(parms);
   TR::ParameterSymbol *parm;

   bool first = true;
   int32_t nextParmNum = staticMethod ? 0 : 1;

   parm = pi.getFirst();
   if (!staticMethod) parm = pi.getNext();

   char name[MAX_NAME];

   for (; parm; parm = pi.getNext())
      {
      getAutoOrParmName(parm, name, false);

      if (!first) ir.print(", ");
      ir.print("%s %s", getTypeName(parm->getDataType()), name);
      first = false;
      nextParmNum++;
      }


   CS2::ArrayOf<gpuParameter, TR::Allocator>::Cursor pit(gpuParameterMap);
   for (pit.SetToFirst(); pit.Valid(); pit.SetToNext())
      {
      getParmName(pit->_parmSlot, name, false);

      if (!first) ir.print(", ");
      ir.print("%s %s", getTypeName(pit->_hostSymRef->getSymbol()->getDataType()), name);
      first = false;
      nextParmNum++;
      }

   int numParms = nextParmNum  - (staticMethod ? 0 : 1);

   ir.print("%s%s%s %%ExceptionKind",
            numParms > 0 ? ", " : "",
            self()->comp()->isGPUCompilation() ? "" : "i32 %startInclusive, i32 %endExclusive, ",
            getTypeName(TR::Address));

   ir.print(") {\n");
   ir.print("entry:\n");

   pi.reset();
   parm = pi.getFirst();
   if (!staticMethod) parm = pi.getNext();

   first = true;
   for (; parm; parm = pi.getNext())
      {
      char name[MAX_NAME];
      getAutoOrParmName(parm, name);
      ir.print("  %s = alloca %s, align %d\n",
                          name,
                          getTypeName(parm->getDataType()),
                          parm->getSize());

      char origName[MAX_NAME];
      getAutoOrParmName(parm, origName, false);
      ir.print("  store %s %s, %s* %s, align %d\n",
                          getTypeName(parm->getDataType()),
                          origName,
                          getTypeName(parm->getDataType()),
                          name,
                          parm->getSize());
      }


   for (pit.SetToFirst(); pit.Valid(); pit.SetToNext())
      {
      TR::Symbol *sym = pit->_hostSymRef->getSymbol();
      char addrName[MAX_NAME+2];
      getParmName(pit->_parmSlot, addrName);
      ir.print("  %s = alloca %s, align %d\n",
                          addrName,
                          getTypeName(sym->getDataType()),
                          sym->getSize());

      char parmName[MAX_NAME];
      getParmName(pit->_parmSlot, parmName, false);
      if (sym->getDataType() == TR::Address)
         {
         if (isbufferalign)
            {
            char name[MAX_NAME];
            strcpy(name, parmName);
            strcat(parmName, ".t");
            ir.print("  %s = getelementptr inbounds i8* %s, i32 %d\n",
                     parmName,
                     name,
                     GPUAlignment - TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
            }
         ir.print("  %%inv_%s_header = call {}* @llvm.invariant.start(i64 %d, i8* %s)\n",
                                        &parmName[1], self()->objectHeaderInvariant(), parmName);
         }
      ir.print("  store %s %s, %s* %s, align %d\n",
                          getTypeName(sym->getDataType()),
                          parmName,
                          getTypeName(sym->getDataType()),
                          addrName,
                          sym->getSize());

      }


   ListIterator<TR::AutomaticSymbol> ai(autos);
   uint16_t liveLocalIndex = 0;
   for (TR::AutomaticSymbol *a = ai.getFirst(); a != NULL; a = ai.getNext())
      {
      ir.print("  %%a%d.addr = alloca %s, align %d\n",
                  liveLocalIndex,
                  getTypeName(a->getDataType()),
                  a->getSize());
      a->setLiveLocalIndex(liveLocalIndex++, 0);
      }


   if (!self()->comp()->isGPUCompilation())
      {
      ir.print("  %%0 = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.x()\n");
      ir.print("  %%1 = call i32 @llvm.nvvm.read.ptx.sreg.ntid.x()\n");
      ir.print("  %%2 = mul i32 %%0, %%1\n");
      ir.print("  %%3 = call i32 @llvm.nvvm.read.ptx.sreg.tid.x()\n");
      ir.print("  %%4 = add i32 %%2, %%3\n");
      ir.print("  %%5 = add i32 %%startInclusive, %%4\n");
      ir.print("  store i32 %%5, i32* %%%s.addr, align 4\n", "a0");
      ir.print("  %%6 = icmp slt i32 %%5, %%endExclusive\n");
      ir.print("  br i1 %%6, label %%block_%d, label %%block_0\n", firstBlock);
      ir.print("block_0:\n");
      self()->dumpInvariant(pit, ir, isbufferalign);
      ir.print("  ret void\n");

      _gpuNodeCount = 7;
      }

   // print all trees
   visitCount = self()->comp()->incVisitCount();

   for (TR::TreeTop * tree = firstTreeTop; tree != lastTreeTop; tree = tree->getNextTreeTop())
      {
      TR::Node *node = tree->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
          currentBlock = node->getBlock()->getNumber();

      if (blocksInLoop && !blocksInLoop->ValueAt(currentBlock))
          continue;

      // don't print the backedge
      if (node->getOpCode().isBranch() &&
          node->getBranchDestination()->getNode()->getBlock()->getNumber() == firstBlock)
          {
          self()->dumpInvariant(pit, ir, isbufferalign);
          ir.print("  ret void\n");
          continue;
          }

      result = self()->printNVVMIR(ir, tree->getNode(), loop, &targetBlocks, visitCount, &sharedMemory, nextParmNum, errorNode);
      if (result != GPUSuccess)
         {
         return result;
         }
      }

   if (_gpuReturnType == TR::NoType)
      {
      self()->dumpInvariant(pit, ir, isbufferalign);
      ir.print("  ret void\n");
      }

   _gpuNodeCount++;

   if (_gpuNeedNullCheckArguments_vector != 0)
      {
      ir.print("; needNullCheckArguments_vector=");
      int32_t len = sizeof(uint64_t) * CHAR_BIT;
      for (int32_t i = len - 1; i >= 0; i--)
         {
         ir.print("%u", (_gpuNeedNullCheckArguments_vector >> (uint64_t)i) & 1);
         }
      ir.print("\n");
      }

   if (_gpuHasNullCheck)
      {
      ir.print("NullException:\n");
      //ir.print("  store i32 1, i32 addrspace(1)* @_ExceptionKind, align 4\n");
      ir.print("  %%%d = bitcast i8* %%ExceptionKind to i32*\n", _gpuNodeCount);
      ir.print("  store i32 %d, i32 * %%%d, align 4\n", GPUNullCheck, _gpuNodeCount++);
      self()->dumpInvariant(pit, ir, isbufferalign);
      ir.print("  ret void\n");
      }
   if (_gpuHasBndCheck)
      {
      ir.print("BndException:\n");
      //ir.print("  store i32 2, i32 addrspace(1)* @_ExceptionKind, align 4\n");
      ir.print("  %%%d = bitcast i8* %%ExceptionKind to i32*\n", _gpuNodeCount);
      ir.print("  store i32 %d, i32 * %%%d, align 4\n", GPUBndCheck, _gpuNodeCount++);
      self()->dumpInvariant(pit, ir, isbufferalign);
      ir.print("  ret void\n");
      }
   if (_gpuHasDivCheck)
      {
      ir.print("DivException:\n");
      //ir.print("  store i32 3, i32 addrspace(1)* @_ExceptionKind, align 4\n");
      ir.print("  %%%d = bitcast i8* %%ExceptionKind to i32*\n", _gpuNodeCount);
      ir.print("  store i32 %d, i32 * %%%d, align 4\n", GPUDivException, _gpuNodeCount++);
      self()->dumpInvariant(pit, ir, isbufferalign);
      ir.print("  ret void\n");
      }

   ir.print("}\n");

   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.ctaid.x() nounwind readnone\n");
   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.ctaid.y() nounwind readnone\n");
   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.ctaid.z() nounwind readnone\n");

   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.ntid.x() nounwind readnone\n");
   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.ntid.y() nounwind readnone\n");
   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.ntid.z() nounwind readnone\n");

   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.tid.x() nounwind readnone\n");
   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.tid.y() nounwind readnone\n");
   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.tid.z() nounwind readnone\n");

   if (self()->comp()->getOptions()->getEnableGPU(TR_EnableGPUEnableMath))
      {
      ir.print("declare double @__nv_sin(double)\n");
      ir.print("declare double @__nv_cos(double)\n");
      ir.print("declare double @__nv_sqrt(double)\n");
      ir.print("declare double @__nv_log(double)\n");
      ir.print("declare double @__nv_exp(double)\n");
      ir.print("declare double @__nv_fabs(double)\n");
      ir.print("declare float @__nv_fabsf(float)\n");
      }

   ir.print("declare void @llvm.nvvm.barrier0() nounwind readnone\n");

   ir.print("!10 = metadata !{i32    0}\n");
   ir.print("!11 = metadata !{i32    1}\n");
   ir.print("!12 = metadata !{i32    2}\n");
   ir.print("!14 = metadata !{i32    4}\n");
   ir.print("!18 = metadata !{i32    8}\n");

   ir.print("!nvvmir.version = !{!0}\n");
   ir.print("!0 = metadata !{i32 1, i32 0}\n");

   ir.print("!nvvm.annotations = !{!1}\n");
   ir.print("!1 = metadata !{%s (", getTypeName(_gpuReturnType));
   pi.reset();
   parm = pi.getFirst();
   if (!staticMethod) parm = pi.getNext();

   first = true;
   for (; parm; parm = pi.getNext())
      {
      if (!first) ir.print(", ");
      ir.print("%s", getTypeName(parm->getDataType()));
      first = false;
      }

   for (pit.SetToFirst(); pit.Valid(); pit.SetToNext())
      {
      TR::Symbol *sym = pit->_hostSymRef->getSymbol();
      if (!first) ir.print(", ");
      ir.print("%s", getTypeName(sym->getDataType()));
      first = false;
      }

   ir.print("%s%s%s",
            numParms > 0 ? ", " : "",
            self()->comp()->isGPUCompilation() ? "" : "i32, i32, ",  // startInclusive, endExclusive
            getTypeName(TR::Address));   // for ExceptionCheck

   ir.print(")* @test%d, metadata !\"kernel\", i32 1}\n", gpuPtxCount);

   nvvmIR = ir.getString();

   traceNVVMIR(self()->comp(), nvvmIR);

   //if any of these are set, it means this kernel may trigger a Java exception
   *hasExceptionChecks = (_gpuHasNullCheck || _gpuHasBndCheck || _gpuHasDivCheck);

   return GPUSuccess;
   }


void
J9::CodeGenerator::generateGPU()
   {
   if (self()->comp()->isGPUCompilation())
      {
      char *programSource;
      TR::Node *errorNode;
      GPUResult result;
      TR::ResolvedMethodSymbol *method = self()->comp()->getJittedMethodSymbol();

      {
      TR::StackMemoryRegion stackMemoryRegion(*self()->trMemory());

      result = self()->dumpNVVMIR(self()->comp()->getStartTree(), self()->comp()->findLastTree(),
                          NULL,
                          NULL,
                          &method->getAutomaticList(),
                          &method->getParameterList(),
                          false, // TODO: check if method is static
                          programSource, errorNode, 0, 0); //gpuPtxCount is not applicable here so it is always set to 0.

      } // scope of the stack memory region

      self()->comp()->getOptimizationPlan()->setGPUResult(result);

      if (result == GPUSuccess)
         {
         self()->comp()->getOptimizationPlan()->setGPUIR(programSource);
         }

      if (!self()->comp()->isGPUCompileCPUCode())
         return;

      TR::CFG *cfg = self()->comp()->getFlowGraph();
      TR::Block *startBlock = self()->comp()->getStartBlock();
      startBlock->split(self()->comp()->getStartTree()->getNextTreeTop(), cfg, false, false);

      ListAppender<TR::ParameterSymbol> la(&method->getParameterList());
      la.empty(); // empty current parameter list

      TR::ParameterSymbol *parmSymbol;
      int slot = 0;

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Address);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setLinkageRegisterIndex(0);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Address);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setLinkageRegisterIndex(1);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Address);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setLinkageRegisterIndex(2);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Address);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setLinkageRegisterIndex(3);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setLinkageRegisterIndex(4);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setLinkageRegisterIndex(5);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setLinkageRegisterIndex(6);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setLinkageRegisterIndex(7);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);


      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Address);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      TR::Node *callNode, *parm;
      TR::SymbolReference *parmSymRef;
      callNode = TR::Node::create(self()->comp()->getStartTree()->getNode(), TR::icall, 13);

      parm = TR::Node::create(callNode, TR::aload, 0);  // vmThread
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 0, TR::Address);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(0, parm);

      parm = TR::Node::create(callNode, TR::aload, 0);   // method
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 1, TR::Address);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(1, parm);

      parm = TR::Node::create(callNode, TR::aload, 0);   // programSource
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 2, TR::Address);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(2, parm);

      parm = TR::Node::create(callNode, TR::aload, 0);   // invokeObject
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 3, TR::Address);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(3, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // deviceId
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 4, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(4, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // gridDimX
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 5, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(5, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // gridDimY
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 6, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(6, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // gridDimZ
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 7, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(7, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // blockDimX
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 8, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(8, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // blockDimY
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 9, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(9, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // blockDimZ
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 10, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(10, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // argCount
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 11, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(11, parm);

      parm = TR::Node::create(callNode, TR::aload, 0); // args
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 12, TR::Address);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(12, parm);

      TR::SymbolReference *helper = self()->comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_callGPU, false, false, false);
      helper->getSymbol()->castToMethodSymbol()->setLinkage(TR_System);
      callNode->setSymbolReference(helper);
      TR::Node *treetop = TR::Node::create(callNode, TR::treetop, 1);
      treetop->setAndIncChild(0, callNode);
      TR::TreeTop *callTreeTop = TR::TreeTop::create(self()->comp(), treetop);
      self()->comp()->getStartTree()->insertAfter(callTreeTop);

      TR::Node *returnNode = TR::Node::create(callNode, TR::ireturn, 1);   // TODO: handle mismatching returns
      returnNode->setAndIncChild(0, callNode);
      TR::TreeTop *returnTreeTop = TR::TreeTop::create(self()->comp(), returnNode);
      callTreeTop->insertAfter(returnTreeTop);

      }
   }

uintptrj_t
J9::CodeGenerator::objectLengthOffset()
   {
   return self()->fe()->getOffsetOfContiguousArraySizeField();
   }

uintptrj_t
J9::CodeGenerator::objectHeaderInvariant()
   {
   return self()->objectLengthOffset() + 4 /*length*/ ;
   }

