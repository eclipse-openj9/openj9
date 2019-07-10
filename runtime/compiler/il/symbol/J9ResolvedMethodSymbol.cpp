/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "compile/Compilation.hpp"


J9::ResolvedMethodSymbol::ResolvedMethodSymbol(TR_ResolvedMethod *method, TR::Compilation *comp) :
   OMR::ResolvedMethodSymbolConnector(method, comp)
   {

   if ((TR::Compiler->target.cpu.getSupportsHardwareRound() &&
        ((method->getRecognizedMethod() == TR::java_lang_Math_floor) ||
         (method->getRecognizedMethod() == TR::java_lang_StrictMath_floor) ||
         (method->getRecognizedMethod() == TR::java_lang_Math_ceil) ||
         (method->getRecognizedMethod() == TR::java_lang_StrictMath_ceil))) ||
       (TR::Compiler->target.cpu.getSupportsHardwareCopySign() &&
        ((method->getRecognizedMethod() == TR::java_lang_Math_copySign_F) ||
         (method->getRecognizedMethod() == TR::java_lang_Math_copySign_D))))
      {
      self()->setCanReplaceWithHWInstr(true);
      }

   if (method->isJNINative())
      {
      self()->setJNI();
#if defined(TR_TARGET_POWER)
      switch(method->getRecognizedMethod())
         {
         case TR::java_lang_Math_acos:
         case TR::java_lang_Math_asin:
         case TR::java_lang_Math_atan:
         case TR::java_lang_Math_atan2:
         case TR::java_lang_Math_cbrt:
         case TR::java_lang_Math_ceil:
         case TR::java_lang_Math_copySign_F:
         case TR::java_lang_Math_copySign_D:
         case TR::java_lang_Math_cos:
         case TR::java_lang_Math_cosh:
         case TR::java_lang_Math_exp:
         case TR::java_lang_Math_expm1:
         case TR::java_lang_Math_floor:
         case TR::java_lang_Math_hypot:
         case TR::java_lang_Math_IEEEremainder:
         case TR::java_lang_Math_log:
         case TR::java_lang_Math_log10:
         case TR::java_lang_Math_log1p:
         case TR::java_lang_Math_max_F:
         case TR::java_lang_Math_max_D:
         case TR::java_lang_Math_min_F:
         case TR::java_lang_Math_min_D:
         case TR::java_lang_Math_nextAfter_F:
         case TR::java_lang_Math_nextAfter_D:
         case TR::java_lang_Math_pow:
         case TR::java_lang_Math_rint:
         case TR::java_lang_Math_round_F:
         case TR::java_lang_Math_round_D:
         case TR::java_lang_Math_scalb_F:
         case TR::java_lang_Math_scalb_D:
         case TR::java_lang_Math_sin:
         case TR::java_lang_Math_sinh:
         case TR::java_lang_Math_sqrt:
         case TR::java_lang_Math_tan:
         case TR::java_lang_Math_tanh:
         case TR::java_lang_StrictMath_acos:
         case TR::java_lang_StrictMath_asin:
         case TR::java_lang_StrictMath_atan:
         case TR::java_lang_StrictMath_atan2:
         case TR::java_lang_StrictMath_cbrt:
         case TR::java_lang_StrictMath_ceil:
         case TR::java_lang_StrictMath_copySign_F:
         case TR::java_lang_StrictMath_copySign_D:
         case TR::java_lang_StrictMath_cos:
         case TR::java_lang_StrictMath_cosh:
         case TR::java_lang_StrictMath_exp:
         case TR::java_lang_StrictMath_expm1:
         case TR::java_lang_StrictMath_floor:
         case TR::java_lang_StrictMath_hypot:
         case TR::java_lang_StrictMath_IEEEremainder:
         case TR::java_lang_StrictMath_log:
         case TR::java_lang_StrictMath_log10:
         case TR::java_lang_StrictMath_log1p:
         case TR::java_lang_StrictMath_max_F:
         case TR::java_lang_StrictMath_max_D:
         case TR::java_lang_StrictMath_min_F:
         case TR::java_lang_StrictMath_min_D:
         case TR::java_lang_StrictMath_nextAfter_F:
         case TR::java_lang_StrictMath_nextAfter_D:
         case TR::java_lang_StrictMath_pow:
         case TR::java_lang_StrictMath_rint:
         case TR::java_lang_StrictMath_round_F:
         case TR::java_lang_StrictMath_round_D:
         case TR::java_lang_StrictMath_scalb_F:
         case TR::java_lang_StrictMath_scalb_D:
         case TR::java_lang_StrictMath_sin:
         case TR::java_lang_StrictMath_sinh:
         case TR::java_lang_StrictMath_sqrt:
         case TR::java_lang_StrictMath_tan:
         case TR::java_lang_StrictMath_tanh:
            setCanDirectNativeCall(true);
            break;
         default:
            break;
         }
#endif // TR_TARGET_POWER
      }

   }

TR::KnownObjectTable::Index
J9::ResolvedMethodSymbol::getKnownObjectIndexForParm(int32_t ordinal)
   {
   TR::KnownObjectTable::Index index = TR::KnownObjectTable::UNKNOWN;
   if (ordinal == 0)
      {
      if (self()->getResolvedMethod()->convertToMethod()->isArchetypeSpecimen())
         {
         TR::KnownObjectTable *knot = self()->comp()->getOrCreateKnownObjectTable();
         if (knot)
            index = knot->getExistingIndexAt(self()->getResolvedMethod()->getMethodHandleLocation());
         }
      }
   return index;
   }
