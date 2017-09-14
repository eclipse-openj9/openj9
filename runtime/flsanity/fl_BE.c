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
#define true 1
#define false 0
#include "j9comp.h"
#include "floatsanityg.h"

typedef void (* testGroup)();
typedef struct range {
	int lo;
	int hi;
}range;

int totalNumTest=0, totalTestsPassed=0, totalTestsFailed=0;


   int isInRange(int grpNum, range *ranges, int numRanges) {
      int i=0, found=0;
      for(i=0; i<numRanges; i++) {
         if (grpNum >= ranges[i].lo && grpNum <= ranges[i].hi ) {
            return 1;
         }
      }
      return found;
   }

   range *initRanges(int argc, char *argsv[], int numTests) {
      range *rs;
      int i;
      rs = malloc(sizeof(range)*argc);
      memset(rs, 0, sizeof(range)*argc);
      if (argc > 1) {
         for(i=1; i<argc; i++) {
            sscanf(argsv[i],"%d-%d",&rs[i-1].lo, &rs[i-1].hi);
            if (rs[i-1].hi==0) rs[i-1].hi = rs[i-1].lo;
         }
         rs[argc-1].lo = -1;
         rs[argc-1].hi = -1;
      } else {
         rs[0].lo = 0;
         rs[0].hi = numTests;
      }
      return rs;
   }

   int main(int argc, char *argv[]) {
      int numArgs = argc;
      char **args = argv;
      range *rs;
      int i=0, numGroups=61;
      int numRanges = argc;
      testGroup groups []= {

         group_JBd2f,
         group_JBd2i,
         group_JBd2l,
         group_JBdadd,
         group_JBdcmpg,
         group_JBdcmpl,
         group_JBddiv,
         group_JBdmul,
         group_JBdneg,
         group_JBdrem,
         group_JBdsub,
         group_JBf2d,
         group_JBf2i,
         group_JBf2l,
         group_JBfadd,
         group_JBfcmpg,
         group_JBfcmpl,
         group_JBfdiv,
         group_JBfmul,
         group_JBfneg,
         group_JBfrem,
         group_JBfsub,
         group_JBi2d,
         group_JBi2f,
         group_JBl2d,
         group_JBl2f,
         group_java_lang_Math_IEEEremainder_double,
         group_java_lang_Math_abs_double,
         group_java_lang_Math_abs_float,
         group_java_lang_Math_acos,
         group_java_lang_Math_asin,
         group_java_lang_Math_atan,
         group_java_lang_Math_atan2,
         group_java_lang_Math_ceil_double,
         group_java_lang_Math_cos,
         group_java_lang_Math_exp,
         group_java_lang_Math_floor_double,
         group_java_lang_Math_log,
         group_java_lang_Math_max_double_double,
         group_java_lang_Math_max_float_float,
         group_java_lang_Math_min_double_double,
         group_java_lang_Math_min_float_float,
         group_java_lang_Math_pow,
         group_java_lang_Math_rint_double,
         group_java_lang_Math_round_double,
         group_java_lang_Math_round_float,
         group_java_lang_Math_sin,
         group_java_lang_Math_sqrt_double,
         group_java_lang_Math_tan,
         group_java_lang_Math_toDegrees_double,
         group_java_lang_Math_toRadians_double,
         /* when adding or removing - ensure you adjust the numGroups value above */
      };

#ifdef IBM_ATOE
	iconv_init();
#endif
      rs = initRanges(argc, argv, numGroups);
      initializeFPU();
      for (i=0; i<numGroups; i++) {
         if (isInRange(i, rs, numRanges)) {
            groups[i]();
         }
      }
      printf("[%d] %s Passed: %d Failed: %d Total: %d \n", numGroups, "floatsanityg", totalTestsPassed, totalTestsFailed, totalNumTest);

      return 0;
}

