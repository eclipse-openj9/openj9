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

#ifndef J9BYTECODEENUMS_INCL
#define J9BYTECODEENUMS_INCL


enum TR_J9ByteCode
   {
   J9BCnop,
   J9BCaconstnull,
   J9BCiconstm1,
   J9BCiconst0, J9BCiconst1, J9BCiconst2, J9BCiconst3, J9BCiconst4, J9BCiconst5, 
   J9BClconst0, J9BClconst1, 
   J9BCfconst0, J9BCfconst1, J9BCfconst2,
   J9BCdconst0, J9BCdconst1,
   J9BCbipush, J9BCsipush,
   J9BCldc, J9BCldcw, J9BCldc2lw, J9BCldc2dw,
   J9BCiload, J9BClload, J9BCfload, J9BCdload, J9BCaload,
   J9BCiload0, J9BCiload1, J9BCiload2, J9BCiload3, 
   J9BClload0, J9BClload1, J9BClload2, J9BClload3,
   J9BCfload0, J9BCfload1, J9BCfload2, J9BCfload3,
   J9BCdload0, J9BCdload1, J9BCdload2, J9BCdload3,
   J9BCaload0, J9BCaload1, J9BCaload2, J9BCaload3,
   J9BCiaload, J9BClaload, J9BCfaload, J9BCdaload, J9BCaaload, J9BCbaload, J9BCcaload, J9BCsaload,
   J9BCiloadw, J9BClloadw, J9BCfloadw, J9BCdloadw, J9BCaloadw, 
   J9BCistore, J9BClstore, J9BCfstore, J9BCdstore, J9BCastore,
   J9BCistorew, J9BClstorew, J9BCfstorew, J9BCdstorew, J9BCastorew,
   J9BCistore0, J9BCistore1, J9BCistore2, J9BCistore3,
   J9BClstore0, J9BClstore1, J9BClstore2, J9BClstore3,
   J9BCfstore0, J9BCfstore1, J9BCfstore2, J9BCfstore3,
   J9BCdstore0, J9BCdstore1, J9BCdstore2, J9BCdstore3,
   J9BCastore0, J9BCastore1, J9BCastore2, J9BCastore3,
   J9BCiastore, J9BClastore, J9BCfastore, J9BCdastore, J9BCaastore, J9BCbastore, J9BCcastore, J9BCsastore,
   J9BCpop, J9BCpop2,
   J9BCdup, J9BCdupx1, J9BCdupx2, J9BCdup2, J9BCdup2x1, J9BCdup2x2,
   J9BCswap,
   J9BCiadd, J9BCladd, J9BCfadd, J9BCdadd,
   J9BCisub, J9BClsub, J9BCfsub, J9BCdsub,
   J9BCimul, J9BClmul, J9BCfmul, J9BCdmul,
   J9BCidiv, J9BCldiv, J9BCfdiv, J9BCddiv,
   J9BCirem, J9BClrem, J9BCfrem, J9BCdrem,
   J9BCineg, J9BClneg, J9BCfneg, J9BCdneg,
   J9BCishl, J9BClshl, J9BCishr, J9BClshr, J9BCiushr, J9BClushr,
   J9BCiand, J9BCland,
   J9BCior, J9BClor,
   J9BCixor, J9BClxor,
   J9BCiinc, J9BCiincw, 
   J9BCi2l, J9BCi2f, J9BCi2d, 
   J9BCl2i, J9BCl2f, J9BCl2d, J9BCf2i, J9BCf2l, J9BCf2d,
   J9BCd2i, J9BCd2l, J9BCd2f,
   J9BCi2b, J9BCi2c, J9BCi2s,
   J9BClcmp, J9BCfcmpl, J9BCfcmpg, J9BCdcmpl, J9BCdcmpg,
   J9BCifeq, J9BCifne, J9BCiflt, J9BCifge, J9BCifgt, J9BCifle,
   J9BCificmpeq, J9BCificmpne, J9BCificmplt, J9BCificmpge, J9BCificmpgt, J9BCificmple, J9BCifacmpeq, J9BCifacmpne,
   J9BCifnull, J9BCifnonnull,
   J9BCgoto, 
   J9BCgotow, 
   J9BCtableswitch, J9BClookupswitch,
   J9BCgenericReturn,
   J9BCgetstatic, J9BCputstatic,
   J9BCgetfield, J9BCputfield,
   J9BCinvokevirtual, J9BCinvokespecial, J9BCinvokestatic, J9BCinvokeinterface, J9BCinvokedynamic, J9BCinvokehandle, J9BCinvokehandlegeneric,J9BCinvokespecialsplit, 

   /** \brief
    *      Pops 1 int32_t argument off the stack and truncates to a uint16_t.
    */
	J9BCReturnC,

	/** \brief
    *      Pops 1 int32_t argument off the stack and truncates to a int16_t.
    */
	J9BCReturnS,

	/** \brief
    *      Pops 1 int32_t argument off the stack and truncates to a int8_t.
    */
	J9BCReturnB,

	/** \brief
    *      Pops 1 int32_t argument off the stack returns the single lowest order bit.
    */
	J9BCReturnZ,

	J9BCinvokestaticsplit, J9BCinvokeinterface2,
   J9BCnew, J9BCnewarray, J9BCanewarray, J9BCmultianewarray,
   J9BCarraylength,
   J9BCathrow,
   J9BCcheckcast,
   J9BCinstanceof,
   J9BCmonitorenter, J9BCmonitorexit,
   J9BCwide,
   J9BCasyncCheck,
   J9BCdefaultvalue,
   J9BCwithfield,
   J9BCbreakpoint,
   J9BCunknown
   };

#endif
