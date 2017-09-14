/*******************************************************************************
 * Copyright (c) 2002, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include "j9comp.h"
#include "fltconst.h"
#include "fltdmath.h"

#define FLWORD(dp) (*((U_32 *)dp))
#define HIWORD(dp) HIGH_U32_FROM_DBL_PTR(dp)
#define LOWORD(dp) LOW_U32_FROM_DBL_PTR(dp)
#define IS_DNAN(dp) IS_NAN_DBL_PTR(dp)
#define IS_SNAN(fp)  IS_NAN_SNGL_PTR(fp)

#if defined(WIN32) || defined(LINUX) 
#include <stdio.h>
#endif

extern int initializeFPU(void);

extern int totalNumTest, totalTestsPassed, totalTestsFailed;
   float JBd2f ( double op_0 );
   int JBd2i ( double op_0 );
   I_64 JBd2l ( double op_0 );
   double JBdadd ( double op_0, double op_1 );
   int JBdcmpg ( double op_0, double op_1 );
   int JBdcmpl ( double op_0, double op_1 );
   double JBddiv ( double op_0, double op_1 );
   double JBdmul ( double op_0, double op_1 );
   double JBdneg ( double op_0 );
   double JBdrem ( double op_0, double op_1 );
   double JBdsub ( double op_0, double op_1 );
   double JBf2d ( float op_0 );
   int JBf2i ( float op_0 );
   I_64 JBf2l ( float op_0 );
   float JBfadd ( float op_0, float op_1 );
   int JBfcmpg ( float op_0, float op_1 );
   int JBfcmpl ( float op_0, float op_1 );
   float JBfdiv ( float op_0, float op_1 );
   float JBfmul ( float op_0, float op_1 );
   float JBfneg ( float op_0 );
   float JBfrem ( float op_0, float op_1 );
   float JBfsub ( float op_0, float op_1 );
   double JBi2d ( int op_0 );
   float JBi2f ( int op_0 );
   double JBl2d ( I_64 op_0 );
   float JBl2f ( I_64 op_0 );
   double java_lang_Math_IEEEremainder_double ( double op_0, double op_1 );
   double java_lang_Math_abs_double ( double op_0 );
   float java_lang_Math_abs_float ( float op_0 );
   double java_lang_Math_acos ( double op_0 );
   double java_lang_Math_asin ( double op_0 );
   double java_lang_Math_atan ( double op_0 );
   double java_lang_Math_atan2 ( double op_0, double op_1 );
   double java_lang_Math_ceil_double ( double op_0 );
   double java_lang_Math_cos ( double op_0 );
   double java_lang_Math_exp ( double op_0 );
   double java_lang_Math_floor_double ( double op_0 );
   double java_lang_Math_log ( double op_0 );
   double java_lang_Math_max_double_double ( double op_0, double op_1 );
   float java_lang_Math_max_float_float ( float op_0, float op_1 );
   double java_lang_Math_min_double_double ( double op_0, double op_1 );
   float java_lang_Math_min_float_float ( float op_0, float op_1 );
   double java_lang_Math_pow ( double op_0, double op_1 );
   double java_lang_Math_rint_double ( double op_0 );
   I_64 java_lang_Math_round_double ( double op_0 );
   int java_lang_Math_round_float ( float op_0 );
   double java_lang_Math_sin ( double op_0 );
   double java_lang_Math_sqrt_double ( double op_0 );
   double java_lang_Math_tan ( double op_0 );
   double java_lang_Math_toDegrees_double ( double op_0 );
   double java_lang_Math_toRadians_double ( double op_0 );
void group_JBd2f();
void group_JBd2i();
void group_JBd2l();
void group_JBdadd();
void group_JBdcmpg();
void group_JBdcmpl();
void group_JBddiv();
void group_JBdmul();
void group_JBdneg();
void group_JBdrem();
void group_JBdsub();
void group_JBf2d();
void group_JBf2i();
void group_JBf2l();
void group_JBfadd();
void group_JBfcmpg();
void group_JBfcmpl();
void group_JBfdiv();
void group_JBfmul();
void group_JBfneg();
void group_JBfrem();
void group_JBfsub();
void group_JBi2d();
void group_JBi2f();
void group_JBl2d();
void group_JBl2f();
void group_java_lang_Math_IEEEremainder_double();
void group_java_lang_Math_abs_double();
void group_java_lang_Math_abs_float();
void group_java_lang_Math_acos();
void group_java_lang_Math_asin();
void group_java_lang_Math_atan();
void group_java_lang_Math_atan2();
void group_java_lang_Math_ceil_double();
void group_java_lang_Math_cos();
void group_java_lang_Math_exp();
void group_java_lang_Math_floor_double();
void group_java_lang_Math_log();
void group_java_lang_Math_max_double_double();
void group_java_lang_Math_max_float_float();
void group_java_lang_Math_min_double_double();
void group_java_lang_Math_min_float_float();
void group_java_lang_Math_pow();
void group_java_lang_Math_rint_double();
void group_java_lang_Math_round_double();
void group_java_lang_Math_round_float();
void group_java_lang_Math_sin();
void group_java_lang_Math_sqrt_double();
void group_java_lang_Math_tan();
void group_java_lang_Math_toDegrees_double();
void group_java_lang_Math_toRadians_double();
