/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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

#ifndef J9PROTOS_H
#define J9PROTOS_H


#include "j9.h"
#include "j9vmls.h"
#include "cfr.h"
#include "jni.h"



#ifdef J9VM_MULTITHREADED
#include "omrthread.h"
#include "thread_api.h"
#endif

#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
/* This is required by vmfntbl.c so that the LE service CEEHDLR can be called directly from builder */
#include <leawi.h>
#endif


#define SEGMENT_TYPE(flags,type) ((flags & MEMORY_TYPE_MASK) == type)


typedef struct J9Relocation {
    U_8* loadBase;
    U_8* loadTop;
    IDATA relocation;
    UDATA relocationType;
    UDATA relocationInfo;
} J9Relocation;


#define RELOCATION_TYPE_CLASSTABLE  2
#define RELOCATION_TYPE_REMOVED  3
#define RELOCATION_TYPE_UNIQUE  1
#define J9SIZEOF_J9Relocation 20

typedef struct J9RelocationList {
    UDATA containsAllZeroRelocations;
    UDATA relocateROM;
    struct J9Pool* relocations;
    struct J9JavaVM* globalInfo;
    UDATA sortedElements;
    struct J9Relocation** sortedRelocations;
    struct J9Relocation* lastFound;
} J9RelocationList;

#define J9SIZEOF_J9RelocationList 28

typedef struct J9RelocationStruct {
    struct J9VMContext* vmContext;
    struct J9RelocationList* relocationList;
    IDATA fix;
    IDATA flags;
    j9object_t pfClass;
    j9object_t cmClass;
    j9object_t bctClass;
    j9object_t bctCompactClass;
    j9object_t stringClass;
    j9object_t symbolClass;
    j9object_t dbStringClass;
    j9object_t maClass;
    j9object_t cpmClass;
    j9object_t mcClass;
    j9object_t floatClass;
    j9object_t bcaClass;
    j9object_t threadClass;
    UDATA mixedObjectCount;
} J9RelocationStruct;

#define J9SIZEOF_J9RelocationStruct 72


typedef J9RelocationStruct relocateStruct;



#include "bcutil_api.h"

#include "bcverify_api.h"

#include "callconv_api.h"

#include "cassume_api.h"

#include "cfdumper_api.h"

#include "hookable_api.h"

#include "jnichk_api.h"

#include "jniinv_api.h"

#include "rasdump_api.h"

#include "rastrace_api.h"

#include "stackmap_api.h"

#include "exelib_api.h"

#include "util_api.h"

#include "verutil_api.h"

#include "avl_api.h"

#include "hashtable_api.h"

#include "srphashtable_api.h"

#include "verbose_api.h"

#include "vm_api.h"

#include "pool_api.h"

#include "simplepool_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* nlsprims.c */

I_32 numCodeSets (void);



/* X86 for MS compilers, __i386__ for GNU compilers */

#if !defined(J9_SOFT_FLOAT) && (defined(_X86_) || defined (__i386__) || defined(J9HAMMER) || defined(OSX))

#define 	J9_SETUP_FPU_STATE() helperInitializeFPU()

#else

#define 	J9_SETUP_FPU_STATE()

#endif



#ifdef WIN64 

/*HACK */

#undef  J9_SETUP_FPU_STATE

#define 	J9_SETUP_FPU_STATE()

#endif






/* J9SourceJclClearProfile*/

/* J9SourceJclFoundationProfile*/

/* J9SourceJclCldc11MTProfile*/

/* J9SourceJclGatewayProfile*/

/* J9SourceJclGwpProfile*/

/* J9SourceJclMidpNGProfile*/

/* J9SourceJclMidpProfile*/

/* J9SourceJclCdc11Profile*/

/* J9SourceJclPBPro11Profile*/

/* J9SourceJclCdcProfile*/

/* J9SourceJclFoundation11Profile*/

/* J9SourceJclPPro10Profile*/

/* J9SourceJclCldcProfile*/

/* J9SourceJclGatewayResourceManagedProfile*/

/* J9SourceJclMaxProfile*/

/* J9SourceJclPBPro10Profile*/

/* J9SourceJclPPro11Profile*/

/* J9SourceJclCldcNGProfile*/

/* J9SourceJ2SEVersionHeader*/

/* J9SourceVMInterfaceDoxygen EXCLUDED */

/* J9SourceJ9VMInterface EXCLUDED */

/* J9SourceCommonVMInterfaceFiles EXCLUDED */

/* J9SourceCommonJVMTIFiles EXCLUDED */

/* J9SourceCommonJVMTINLS EXCLUDED */

/* J9CommonJvmtiTDF EXCLUDED */


/*================ ASM PROTOTYPES ===================*/

/* J9VMAccessControl*/
#ifndef _J9VMACCESSCONTROL_
#define _J9VMACCESSCONTROL_
extern J9_CFUNC void  internalExitVMToJNI (J9VMThread *currentThread);
extern J9_CFUNC void  internalEnterVMFromJNI (J9VMThread *currentThread);
extern J9_CFUNC void  internalReleaseVMAccess (J9VMThread *vmThread);
extern J9_CFUNC void  internalAcquireVMAccess (J9VMThread *vmThread);
extern J9_CFUNC IDATA  internalTryAcquireVMAccess (J9VMThread *vmThread);
#endif /* _J9VMACCESSCONTROL_ */

/* J9VMStackDumper*/
#ifndef _J9VMSTACKDUMPER_
#define _J9VMSTACKDUMPER_
extern J9_CFUNC void  dumpStackTrace (J9VMThread *currentThread);
extern J9_CFUNC UDATA  genericStackDumpIterator (J9VMThread *currentThread, J9StackWalkState *walkState);
#endif /* _J9VMSTACKDUMPER_ */

/* J9VMThreadFlags*/
#ifndef _J9VMTHREADFLAGS_
#define _J9VMTHREADFLAGS_
extern J9_CFUNC void  clearEventFlag (J9VMThread *vmThread, UDATA flag);
extern J9_CFUNC void  setEventFlag (J9VMThread *vmThread, UDATA flag);
extern J9_CFUNC void  setHaltFlag (J9VMThread *vmThread, UDATA flag);
extern J9_CFUNC void  clearHaltFlag (J9VMThread *vmThread, UDATA flag);
#endif /* _J9VMTHREADFLAGS_ */

/* J9VMCallingConventionJNITests*/
#ifndef _J9INGCONVENTIONJNITESTS_
#define _J9INGCONVENTIONJNITESTS_
extern J9_CFUNC void  JNICallout (UDATA argCount, U_8* argTypes, UDATA returnType, void* returnStorage, void* function);
#endif /* _J9INGCONVENTIONJNITESTS_ */

/* J9VMCallingConventionTests*/
#ifndef _J9INGCONVENTIONTESTS_
#define _J9INGCONVENTIONTESTS_
extern J9_CFUNC I_64  JNICALL C_x_co_FI_J (float arg1, I_32 arg2);
extern J9_CFUNC I_32  JNICALL C_ci_IIII_I (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC char*  JNICALL C_x_co_FJ_L (float arg1, I_64 arg2);
extern J9_CFUNC double  JNICALL C_x_co_FJ_D (float arg1, I_64 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_FL_L (float arg1, const char * arg2);
extern J9_CFUNC char*  JNICALL C_x_co_FD_L (float arg1, double arg2);
extern J9_CFUNC I_32  JNICALL C_co_IIII_I (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC void  setAsmTestName (char *testName);
extern J9_CFUNC char*  JNICALL C_ci_LLLL_L (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_FL_I (float arg1, const char * arg2);
extern J9_CFUNC float  JNICALL C_x_co_FF_F (float arg1, float arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_FL_J (float arg1, const char * arg2);
extern J9_CFUNC char*  JNICALL ASM_ci_L (void);
extern J9_CFUNC float  JNICALL C_co_FFF_F (float arg1, float arg2, float arg3);
extern J9_CFUNC float  JNICALL ASM_ci_FFF_F (float arg1, float arg2, float arg3);
extern J9_CFUNC double  JNICALL C_x_co_FI_D (float arg1, I_32 arg2);
extern J9_CFUNC float  ASM_co_F_F (float arg1);
extern J9_CFUNC float  JNICALL C_x_co_FL_F (float arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_FD_I (float arg1, double arg2);
extern J9_CFUNC double  JNICALL C_x_co_FF_D (float arg1, float arg2);
extern J9_CFUNC double  JNICALL C_x_co_FD_D (float arg1, double arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_FJ_I (float arg1, I_64 arg2);
extern J9_CFUNC I_64  JNICALL ASM_ci_J (void);
extern J9_CFUNC I_32  JNICALL C_x_co_FF_I (float arg1, float arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_FJ_J (float arg1, I_64 arg2);
extern J9_CFUNC double  JNICALL C_x_ci_LLLL_D (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_FD_J (float arg1, double arg2);
extern J9_CFUNC char*  JNICALL ASM_ci_LLLLL_L (const char * arg1, const char * arg2, const char * arg3, const char * arg4, const char * arg5);
extern J9_CFUNC float  JNICALL C_x_co_FI_F (float arg1, I_32 arg2);
extern J9_CFUNC I_32  ASM_x_co_LILI_I (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC char*  JNICALL C_x_co_FI_L (float arg1, I_32 arg2);
extern J9_CFUNC double  ASM_x_co_LJLJ_D (const char * arg1, I_64 arg2, const char * arg3, I_64 arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_FI_I (float arg1, I_32 arg2);
extern J9_CFUNC I_32  ASM_x_co_LDLD_I (const char * arg1, double arg2, const char * arg3, double arg4);
extern J9_CFUNC char*  ASM_x_co_LDLD_L (const char * arg1, double arg2, const char * arg3, double arg4);
extern J9_CFUNC char*  JNICALL C_x_co_FF_L (float arg1, float arg2);
extern J9_CFUNC I_32  ASM_x_co_LFLF_I (const char * arg1, float arg2, const char * arg3, float arg4);
extern J9_CFUNC I_64  JNICALL C_x_ci_LLLL_J (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC float  ASM_x_co_LLLL_F (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC I_32  ASM_x_co_LLLL_I (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC char*  ASM_x_co_LJLJ_L (const char * arg1, I_64 arg2, const char * arg3, I_64 arg4);
extern J9_CFUNC I_32  JNICALL C_x_ci_LILI_I (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC I_32  JNICALL C_co_II_I (I_32 arg1, I_32 arg2);
extern J9_CFUNC I_64  JNICALL C_x_ci_LILI_J (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC char*  JNICALL C_x_ci_LLLL_L (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC float  JNICALL C_x_ci_LLLL_F (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC I_32  JNICALL C_ci_II_I (I_32 arg1, I_32 arg2);
extern J9_CFUNC float  JNICALL C_x_ci_LILI_F (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC I_64  JNICALL ASM_ci_J_J (I_64 arg1);
extern J9_CFUNC float  ASM_x_co_LILI_F (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC I_32  ASM_x_co_LJLJ_I (const char * arg1, I_64 arg2, const char * arg3, I_64 arg4);
extern J9_CFUNC char*  ASM_x_co_LLLL_L (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC I_64  ASM_x_co_LILI_J (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC float  JNICALL C_co_F_F (float arg1);
extern J9_CFUNC double  JNICALL ASM_ci_D_D (double arg1);
extern J9_CFUNC double  ASM_x_co_LFLF_D (const char * arg1, float arg2, const char * arg3, float arg4);
extern J9_CFUNC I_64  ASM_x_co_LFLF_J (const char * arg1, float arg2, const char * arg3, float arg4);
extern J9_CFUNC float  ASM_x_co_LDLD_F (const char * arg1, double arg2, const char * arg3, double arg4);
extern J9_CFUNC I_64  ASM_x_co_LJLJ_J (const char * arg1, I_64 arg2, const char * arg3, I_64 arg4);
extern J9_CFUNC double  ASM_x_co_LLLL_D (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC double  ASM_x_co_LILI_D (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC double  JNICALL C_ci_D_D (double arg1);
extern J9_CFUNC float  ASM_x_co_LJLJ_F (const char * arg1, I_64 arg2, const char * arg3, I_64 arg4);
extern J9_CFUNC char*  ASM_x_co_LFLF_L (const char * arg1, float arg2, const char * arg3, float arg4);
extern J9_CFUNC I_64  JNICALL ASM_ci_JJ_J (I_64 arg1, I_64 arg2);
extern J9_CFUNC I_64  ASM_x_co_LDLD_J (const char * arg1, double arg2, const char * arg3, double arg4);
extern J9_CFUNC I_32  JNICALL C_x_ci_LLLL_I (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC float  ASM_x_co_LFLF_F (const char * arg1, float arg2, const char * arg3, float arg4);
extern J9_CFUNC double  JNICALL C_x_ci_LILI_D (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC I_32  JNICALL C_x_custom_co_IJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJ_I (I_32 arg1, I_64 arg2, I_32 arg3, I_64 arg4, I_32 arg5, I_64 arg6, I_32 arg7, I_64 arg8, I_32 arg9, I_64 arg10, I_32 arg11, I_64 arg12, I_32 arg13, I_64 arg14, I_32 arg15, I_64 arg16, I_32 arg17, I_64 arg18, I_32 arg19, I_64 arg20, I_32 arg21, I_64 arg22, I_32 arg23, I_64 arg24, I_32 arg25, I_64 arg26, I_32 arg27, I_64 arg28, I_32 arg29, I_64 arg30, I_32 arg31, I_64 arg32, I_32 arg33, I_64 arg34, I_32 arg35, I_64 arg36, I_32 arg37, I_64 arg38, I_32 arg39, I_64 arg40, I_32 arg41, I_64 arg42, I_32 arg43, I_64 arg44, I_32 arg45, I_64 arg46, I_32 arg47, I_64 arg48, I_32 arg49, I_64 arg50, I_32 arg51, I_64 arg52, I_32 arg53, I_64 arg54, I_32 arg55, I_64 arg56, I_32 arg57, I_64 arg58, I_32 arg59, I_64 arg60, I_32 arg61, I_64 arg62, I_32 arg63, I_64 arg64, I_32 arg65, I_64 arg66, I_32 arg67, I_64 arg68, I_32 arg69, I_64 arg70, I_32 arg71, I_64 arg72, I_32 arg73, I_64 arg74, I_32 arg75, I_64 arg76, I_32 arg77, I_64 arg78, I_32 arg79, I_64 arg80, I_32 arg81, I_64 arg82, I_32 arg83, I_64 arg84, I_32 arg85, I_64 arg86, I_32 arg87, I_64 arg88, I_32 arg89, I_64 arg90, I_32 arg91, I_64 arg92, I_32 arg93, I_64 arg94, I_32 arg95, I_64 arg96, I_32 arg97, I_64 arg98, I_32 arg99, I_64 arg100, I_32 arg101, I_64 arg102, I_32 arg103, I_64 arg104, I_32 arg105, I_64 arg106, I_32 arg107, I_64 arg108, I_32 arg109, I_64 arg110, I_32 arg111, I_64 arg112, I_32 arg113, I_64 arg114, I_32 arg115, I_64 arg116, I_32 arg117, I_64 arg118, I_32 arg119, I_64 arg120, I_32 arg121, I_64 arg122, I_32 arg123, I_64 arg124, I_32 arg125, I_64 arg126, I_32 arg127, I_64 arg128, I_32 arg129, I_64 arg130, I_32 arg131, I_64 arg132, I_32 arg133, I_64 arg134, I_32 arg135, I_64 arg136, I_32 arg137, I_64 arg138, I_32 arg139, I_64 arg140, I_32 arg141, I_64 arg142, I_32 arg143, I_64 arg144, I_32 arg145, I_64 arg146, I_32 arg147, I_64 arg148, I_32 arg149, I_64 arg150, I_32 arg151, I_64 arg152, I_32 arg153, I_64 arg154, I_32 arg155, I_64 arg156, I_32 arg157, I_64 arg158, I_32 arg159, I_64 arg160, I_32 arg161, I_64 arg162, I_32 arg163, I_64 arg164, I_32 arg165, I_64 arg166, I_32 arg167, I_64 arg168, I_32 arg169, I_64 arg170);
extern J9_CFUNC char*  ASM_x_co_LILI_L (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC char*  JNICALL C_x_ci_LILI_L (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC double  ASM_x_co_LDLD_D (const char * arg1, double arg2, const char * arg3, double arg4);
extern J9_CFUNC I_64  JNICALL C_x_custom_co_FDFDFDFDFD_J (float arg1, double arg2, float arg3, double arg4, float arg5, double arg6, float arg7, double arg8, float arg9, double arg10);
extern J9_CFUNC I_64  ASM_x_co_LLLL_J (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC I_32  ASM_x_co_JIJI_I (I_64 arg1, I_32 arg2, I_64 arg3, I_32 arg4);
extern J9_CFUNC float  ASM_co_FF_F (float arg1, float arg2);
extern J9_CFUNC double  ASM_x_co_JJJJ_D (I_64 arg1, I_64 arg2, I_64 arg3, I_64 arg4);
extern J9_CFUNC I_32  ASM_x_co_JDJD_I (I_64 arg1, double arg2, I_64 arg3, double arg4);
extern J9_CFUNC char*  ASM_x_co_JDJD_L (I_64 arg1, double arg2, I_64 arg3, double arg4);
extern J9_CFUNC I_32  ASM_x_co_JFJF_I (I_64 arg1, float arg2, I_64 arg3, float arg4);
extern J9_CFUNC float  ASM_x_co_JLJL_F (I_64 arg1, const char * arg2, I_64 arg3, const char * arg4);
extern J9_CFUNC double  ASM_co_DD_D (double arg1, double arg2);
extern J9_CFUNC I_32  ASM_x_co_JLJL_I (I_64 arg1, const char * arg2, I_64 arg3, const char * arg4);
extern J9_CFUNC char*  ASM_x_co_JJJJ_L (I_64 arg1, I_64 arg2, I_64 arg3, I_64 arg4);
extern J9_CFUNC char*  ASM_co_LLL_L (const char * arg1, const char * arg2, const char * arg3);
extern J9_CFUNC I_32  JNICALL C_x_custom_co_IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII_I (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4, I_32 arg5, I_32 arg6, I_32 arg7, I_32 arg8, I_32 arg9, I_32 arg10, I_32 arg11, I_32 arg12, I_32 arg13, I_32 arg14, I_32 arg15, I_32 arg16, I_32 arg17, I_32 arg18, I_32 arg19, I_32 arg20, I_32 arg21, I_32 arg22, I_32 arg23, I_32 arg24, I_32 arg25, I_32 arg26, I_32 arg27, I_32 arg28, I_32 arg29, I_32 arg30, I_32 arg31, I_32 arg32, I_32 arg33, I_32 arg34, I_32 arg35, I_32 arg36, I_32 arg37, I_32 arg38, I_32 arg39, I_32 arg40, I_32 arg41, I_32 arg42, I_32 arg43, I_32 arg44, I_32 arg45, I_32 arg46, I_32 arg47, I_32 arg48, I_32 arg49, I_32 arg50, I_32 arg51, I_32 arg52, I_32 arg53, I_32 arg54, I_32 arg55, I_32 arg56, I_32 arg57, I_32 arg58, I_32 arg59, I_32 arg60, I_32 arg61, I_32 arg62, I_32 arg63, I_32 arg64, I_32 arg65, I_32 arg66, I_32 arg67, I_32 arg68, I_32 arg69, I_32 arg70, I_32 arg71, I_32 arg72, I_32 arg73, I_32 arg74, I_32 arg75, I_32 arg76, I_32 arg77, I_32 arg78, I_32 arg79, I_32 arg80, I_32 arg81, I_32 arg82, I_32 arg83, I_32 arg84, I_32 arg85, I_32 arg86, I_32 arg87, I_32 arg88, I_32 arg89, I_32 arg90, I_32 arg91, I_32 arg92, I_32 arg93, I_32 arg94, I_32 arg95, I_32 arg96, I_32 arg97, I_32 arg98, I_32 arg99, I_32 arg100, I_32 arg101, I_32 arg102, I_32 arg103, I_32 arg104, I_32 arg105, I_32 arg106, I_32 arg107, I_32 arg108, I_32 arg109, I_32 arg110, I_32 arg111, I_32 arg112, I_32 arg113, I_32 arg114, I_32 arg115, I_32 arg116, I_32 arg117, I_32 arg118, I_32 arg119, I_32 arg120, I_32 arg121, I_32 arg122, I_32 arg123, I_32 arg124, I_32 arg125, I_32 arg126, I_32 arg127, I_32 arg128, I_32 arg129, I_32 arg130, I_32 arg131, I_32 arg132, I_32 arg133, I_32 arg134, I_32 arg135, I_32 arg136, I_32 arg137, I_32 arg138, I_32 arg139, I_32 arg140, I_32 arg141, I_32 arg142, I_32 arg143, I_32 arg144, I_32 arg145, I_32 arg146, I_32 arg147, I_32 arg148, I_32 arg149, I_32 arg150, I_32 arg151, I_32 arg152, I_32 arg153, I_32 arg154, I_32 arg155, I_32 arg156, I_32 arg157, I_32 arg158, I_32 arg159, I_32 arg160, I_32 arg161, I_32 arg162, I_32 arg163, I_32 arg164, I_32 arg165, I_32 arg166, I_32 arg167, I_32 arg168, I_32 arg169, I_32 arg170, I_32 arg171, I_32 arg172, I_32 arg173, I_32 arg174, I_32 arg175, I_32 arg176, I_32 arg177, I_32 arg178, I_32 arg179, I_32 arg180, I_32 arg181, I_32 arg182, I_32 arg183, I_32 arg184, I_32 arg185, I_32 arg186, I_32 arg187, I_32 arg188, I_32 arg189, I_32 arg190, I_32 arg191, I_32 arg192, I_32 arg193, I_32 arg194, I_32 arg195, I_32 arg196, I_32 arg197, I_32 arg198, I_32 arg199, I_32 arg200, I_32 arg201, I_32 arg202, I_32 arg203, I_32 arg204, I_32 arg205, I_32 arg206, I_32 arg207, I_32 arg208, I_32 arg209, I_32 arg210, I_32 arg211, I_32 arg212, I_32 arg213, I_32 arg214, I_32 arg215, I_32 arg216, I_32 arg217, I_32 arg218, I_32 arg219, I_32 arg220, I_32 arg221, I_32 arg222, I_32 arg223, I_32 arg224, I_32 arg225, I_32 arg226, I_32 arg227, I_32 arg228, I_32 arg229, I_32 arg230, I_32 arg231, I_32 arg232, I_32 arg233, I_32 arg234, I_32 arg235, I_32 arg236, I_32 arg237, I_32 arg238, I_32 arg239, I_32 arg240, I_32 arg241, I_32 arg242, I_32 arg243, I_32 arg244, I_32 arg245, I_32 arg246, I_32 arg247, I_32 arg248, I_32 arg249, I_32 arg250, I_32 arg251, I_32 arg252, I_32 arg253, I_32 arg254, I_32 arg255);
extern J9_CFUNC float  ASM_x_co_JIJI_F (I_64 arg1, I_32 arg2, I_64 arg3, I_32 arg4);
extern J9_CFUNC I_32  ASM_x_co_JJJJ_I (I_64 arg1, I_64 arg2, I_64 arg3, I_64 arg4);
extern J9_CFUNC char*  ASM_x_co_JLJL_L (I_64 arg1, const char * arg2, I_64 arg3, const char * arg4);
extern J9_CFUNC I_64  ASM_x_co_JIJI_J (I_64 arg1, I_32 arg2, I_64 arg3, I_32 arg4);
extern J9_CFUNC double  ASM_x_co_JFJF_D (I_64 arg1, float arg2, I_64 arg3, float arg4);
extern J9_CFUNC I_64  ASM_x_co_JFJF_J (I_64 arg1, float arg2, I_64 arg3, float arg4);
extern J9_CFUNC float  ASM_x_co_JDJD_F (I_64 arg1, double arg2, I_64 arg3, double arg4);
extern J9_CFUNC I_64  ASM_x_co_JJJJ_J (I_64 arg1, I_64 arg2, I_64 arg3, I_64 arg4);
extern J9_CFUNC double  ASM_x_co_JLJL_D (I_64 arg1, const char * arg2, I_64 arg3, const char * arg4);
extern J9_CFUNC double  ASM_x_co_JIJI_D (I_64 arg1, I_32 arg2, I_64 arg3, I_32 arg4);
extern J9_CFUNC float  ASM_x_co_JJJJ_F (I_64 arg1, I_64 arg2, I_64 arg3, I_64 arg4);
extern J9_CFUNC char*  ASM_x_co_JFJF_L (I_64 arg1, float arg2, I_64 arg3, float arg4);
extern J9_CFUNC I_32  ASM_x_custom_co_IJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ_I (I_32 arg1, I_64 arg2, I_64 arg3, I_64 arg4, I_64 arg5, I_64 arg6, I_64 arg7, I_64 arg8, I_64 arg9, I_64 arg10, I_64 arg11, I_64 arg12, I_64 arg13, I_64 arg14, I_64 arg15, I_64 arg16, I_64 arg17, I_64 arg18, I_64 arg19, I_64 arg20, I_64 arg21, I_64 arg22, I_64 arg23, I_64 arg24, I_64 arg25, I_64 arg26, I_64 arg27, I_64 arg28, I_64 arg29, I_64 arg30, I_64 arg31, I_64 arg32, I_64 arg33, I_64 arg34, I_64 arg35, I_64 arg36, I_64 arg37, I_64 arg38, I_64 arg39, I_64 arg40, I_64 arg41, I_64 arg42, I_64 arg43, I_64 arg44, I_64 arg45, I_64 arg46, I_64 arg47, I_64 arg48, I_64 arg49, I_64 arg50, I_64 arg51, I_64 arg52, I_64 arg53, I_64 arg54, I_64 arg55, I_64 arg56, I_64 arg57, I_64 arg58, I_64 arg59, I_64 arg60, I_64 arg61, I_64 arg62, I_64 arg63, I_64 arg64, I_64 arg65, I_64 arg66, I_64 arg67, I_64 arg68, I_64 arg69, I_64 arg70, I_64 arg71, I_64 arg72, I_64 arg73, I_64 arg74, I_64 arg75, I_64 arg76, I_64 arg77, I_64 arg78, I_64 arg79, I_64 arg80, I_64 arg81, I_64 arg82, I_64 arg83, I_64 arg84, I_64 arg85, I_64 arg86, I_64 arg87, I_64 arg88, I_64 arg89, I_64 arg90, I_64 arg91, I_64 arg92, I_64 arg93, I_64 arg94, I_64 arg95, I_64 arg96, I_64 arg97, I_64 arg98, I_64 arg99, I_64 arg100, I_64 arg101, I_64 arg102, I_64 arg103, I_64 arg104, I_64 arg105, I_64 arg106, I_64 arg107, I_64 arg108, I_64 arg109, I_64 arg110, I_64 arg111, I_64 arg112, I_64 arg113, I_64 arg114, I_64 arg115, I_64 arg116, I_64 arg117, I_64 arg118, I_64 arg119, I_64 arg120, I_64 arg121, I_64 arg122, I_64 arg123, I_64 arg124, I_64 arg125, I_64 arg126, I_64 arg127, I_64 arg128);
extern J9_CFUNC I_64  ASM_x_co_JDJD_J (I_64 arg1, double arg2, I_64 arg3, double arg4);
extern J9_CFUNC float  ASM_x_co_JFJF_F (I_64 arg1, float arg2, I_64 arg3, float arg4);
extern J9_CFUNC char*  ASM_x_co_JIJI_L (I_64 arg1, I_32 arg2, I_64 arg3, I_32 arg4);
extern J9_CFUNC float  ASM_co_F (void);
extern J9_CFUNC double  ASM_x_co_JDJD_D (I_64 arg1, double arg2, I_64 arg3, double arg4);
extern J9_CFUNC I_64  ASM_x_co_JLJL_J (I_64 arg1, const char * arg2, I_64 arg3, const char * arg4);
extern J9_CFUNC float  JNICALL C_ci_FF_F (float arg1, float arg2);
extern J9_CFUNC double  JNICALL C_ci_DD_D (double arg1, double arg2);
extern J9_CFUNC char*  ASM_co_L (void);
extern J9_CFUNC char*  JNICALL C_co_L_L (const char * arg1);
extern J9_CFUNC double  JNICALL C_ci_D (void);
extern J9_CFUNC I_32  ASM_x_custom_co_IJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJIJ_I (I_32 arg1, I_64 arg2, I_32 arg3, I_64 arg4, I_32 arg5, I_64 arg6, I_32 arg7, I_64 arg8, I_32 arg9, I_64 arg10, I_32 arg11, I_64 arg12, I_32 arg13, I_64 arg14, I_32 arg15, I_64 arg16, I_32 arg17, I_64 arg18, I_32 arg19, I_64 arg20, I_32 arg21, I_64 arg22, I_32 arg23, I_64 arg24, I_32 arg25, I_64 arg26, I_32 arg27, I_64 arg28, I_32 arg29, I_64 arg30, I_32 arg31, I_64 arg32, I_32 arg33, I_64 arg34, I_32 arg35, I_64 arg36, I_32 arg37, I_64 arg38, I_32 arg39, I_64 arg40, I_32 arg41, I_64 arg42, I_32 arg43, I_64 arg44, I_32 arg45, I_64 arg46, I_32 arg47, I_64 arg48, I_32 arg49, I_64 arg50, I_32 arg51, I_64 arg52, I_32 arg53, I_64 arg54, I_32 arg55, I_64 arg56, I_32 arg57, I_64 arg58, I_32 arg59, I_64 arg60, I_32 arg61, I_64 arg62, I_32 arg63, I_64 arg64, I_32 arg65, I_64 arg66, I_32 arg67, I_64 arg68, I_32 arg69, I_64 arg70, I_32 arg71, I_64 arg72, I_32 arg73, I_64 arg74, I_32 arg75, I_64 arg76, I_32 arg77, I_64 arg78, I_32 arg79, I_64 arg80, I_32 arg81, I_64 arg82, I_32 arg83, I_64 arg84, I_32 arg85, I_64 arg86, I_32 arg87, I_64 arg88, I_32 arg89, I_64 arg90, I_32 arg91, I_64 arg92, I_32 arg93, I_64 arg94, I_32 arg95, I_64 arg96, I_32 arg97, I_64 arg98, I_32 arg99, I_64 arg100, I_32 arg101, I_64 arg102, I_32 arg103, I_64 arg104, I_32 arg105, I_64 arg106, I_32 arg107, I_64 arg108, I_32 arg109, I_64 arg110, I_32 arg111, I_64 arg112, I_32 arg113, I_64 arg114, I_32 arg115, I_64 arg116, I_32 arg117, I_64 arg118, I_32 arg119, I_64 arg120, I_32 arg121, I_64 arg122, I_32 arg123, I_64 arg124, I_32 arg125, I_64 arg126, I_32 arg127, I_64 arg128, I_32 arg129, I_64 arg130, I_32 arg131, I_64 arg132, I_32 arg133, I_64 arg134, I_32 arg135, I_64 arg136, I_32 arg137, I_64 arg138, I_32 arg139, I_64 arg140, I_32 arg141, I_64 arg142, I_32 arg143, I_64 arg144, I_32 arg145, I_64 arg146, I_32 arg147, I_64 arg148, I_32 arg149, I_64 arg150, I_32 arg151, I_64 arg152, I_32 arg153, I_64 arg154, I_32 arg155, I_64 arg156, I_32 arg157, I_64 arg158, I_32 arg159, I_64 arg160, I_32 arg161, I_64 arg162, I_32 arg163, I_64 arg164, I_32 arg165, I_64 arg166, I_32 arg167, I_64 arg168, I_32 arg169, I_64 arg170);
extern J9_CFUNC char*  JNICALL C_co_LL_L (const char * arg1, const char * arg2);
extern J9_CFUNC I_32  ASM_x_co_IIII_I (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC I_32  JNICALL C_x_custom_co_IJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ_I (I_32 arg1, I_64 arg2, I_64 arg3, I_64 arg4, I_64 arg5, I_64 arg6, I_64 arg7, I_64 arg8, I_64 arg9, I_64 arg10, I_64 arg11, I_64 arg12, I_64 arg13, I_64 arg14, I_64 arg15, I_64 arg16, I_64 arg17, I_64 arg18, I_64 arg19, I_64 arg20, I_64 arg21, I_64 arg22, I_64 arg23, I_64 arg24, I_64 arg25, I_64 arg26, I_64 arg27, I_64 arg28, I_64 arg29, I_64 arg30, I_64 arg31, I_64 arg32, I_64 arg33, I_64 arg34, I_64 arg35, I_64 arg36, I_64 arg37, I_64 arg38, I_64 arg39, I_64 arg40, I_64 arg41, I_64 arg42, I_64 arg43, I_64 arg44, I_64 arg45, I_64 arg46, I_64 arg47, I_64 arg48, I_64 arg49, I_64 arg50, I_64 arg51, I_64 arg52, I_64 arg53, I_64 arg54, I_64 arg55, I_64 arg56, I_64 arg57, I_64 arg58, I_64 arg59, I_64 arg60, I_64 arg61, I_64 arg62, I_64 arg63, I_64 arg64, I_64 arg65, I_64 arg66, I_64 arg67, I_64 arg68, I_64 arg69, I_64 arg70, I_64 arg71, I_64 arg72, I_64 arg73, I_64 arg74, I_64 arg75, I_64 arg76, I_64 arg77, I_64 arg78, I_64 arg79, I_64 arg80, I_64 arg81, I_64 arg82, I_64 arg83, I_64 arg84, I_64 arg85, I_64 arg86, I_64 arg87, I_64 arg88, I_64 arg89, I_64 arg90, I_64 arg91, I_64 arg92, I_64 arg93, I_64 arg94, I_64 arg95, I_64 arg96, I_64 arg97, I_64 arg98, I_64 arg99, I_64 arg100, I_64 arg101, I_64 arg102, I_64 arg103, I_64 arg104, I_64 arg105, I_64 arg106, I_64 arg107, I_64 arg108, I_64 arg109, I_64 arg110, I_64 arg111, I_64 arg112, I_64 arg113, I_64 arg114, I_64 arg115, I_64 arg116, I_64 arg117, I_64 arg118, I_64 arg119, I_64 arg120, I_64 arg121, I_64 arg122, I_64 arg123, I_64 arg124, I_64 arg125, I_64 arg126, I_64 arg127, I_64 arg128);
extern J9_CFUNC double  ASM_x_co_IJIJ_D (I_32 arg1, I_64 arg2, I_32 arg3, I_64 arg4);
extern J9_CFUNC I_32  ASM_x_co_IDID_I (I_32 arg1, double arg2, I_32 arg3, double arg4);
extern J9_CFUNC char*  ASM_x_co_IDID_L (I_32 arg1, double arg2, I_32 arg3, double arg4);
extern J9_CFUNC I_32  ASM_x_co_IFIF_I (I_32 arg1, float arg2, I_32 arg3, float arg4);
extern J9_CFUNC float  ASM_x_co_ILIL_F (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC I_32  ASM_x_co_ILIL_I (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC char*  ASM_x_co_IJIJ_L (I_32 arg1, I_64 arg2, I_32 arg3, I_64 arg4);
extern J9_CFUNC I_32  JNICALL C_ci_I_I (I_32 arg1);
extern J9_CFUNC float  JNICALL ASM_ci_F_F (float arg1);
extern J9_CFUNC float  ASM_x_co_IIII_F (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC I_32  ASM_x_co_IJIJ_I (I_32 arg1, I_64 arg2, I_32 arg3, I_64 arg4);
extern J9_CFUNC char*  ASM_x_co_ILIL_L (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC I_64  ASM_x_co_IIII_J (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC I_64  JNICALL C_ci_JJ_J (I_64 arg1, I_64 arg2);
extern J9_CFUNC double  ASM_x_co_IFIF_D (I_32 arg1, float arg2, I_32 arg3, float arg4);
extern J9_CFUNC I_64  ASM_x_co_IFIF_J (I_32 arg1, float arg2, I_32 arg3, float arg4);
extern J9_CFUNC float  ASM_x_co_IDID_F (I_32 arg1, double arg2, I_32 arg3, double arg4);
extern J9_CFUNC double  JNICALL C_co_D_D (double arg1);
extern J9_CFUNC I_64  ASM_x_co_IJIJ_J (I_32 arg1, I_64 arg2, I_32 arg3, I_64 arg4);
extern J9_CFUNC I_64  JNICALL C_ci_J (void);
extern J9_CFUNC double  ASM_x_co_ILIL_D (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC double  ASM_x_co_IIII_D (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC float  ASM_x_co_IJIJ_F (I_32 arg1, I_64 arg2, I_32 arg3, I_64 arg4);
extern J9_CFUNC char*  ASM_x_co_IFIF_L (I_32 arg1, float arg2, I_32 arg3, float arg4);
extern J9_CFUNC I_64  ASM_x_co_IDID_J (I_32 arg1, double arg2, I_32 arg3, double arg4);
extern J9_CFUNC float  ASM_x_co_IFIF_F (I_32 arg1, float arg2, I_32 arg3, float arg4);
extern J9_CFUNC char*  ASM_x_co_IIII_L (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC double  JNICALL C_x_co_ILIL_D (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC double  ASM_x_co_IDID_D (I_32 arg1, double arg2, I_32 arg3, double arg4);
extern J9_CFUNC double  JNICALL C_x_co_IDID_D (I_32 arg1, double arg2, I_32 arg3, double arg4);
extern J9_CFUNC char*  ASM_x_co_LD_L (const char * arg1, double arg2);
extern J9_CFUNC I_64  ASM_x_co_ILIL_J (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC I_64  ASM_x_co_LD_J (const char * arg1, double arg2);
extern J9_CFUNC double  ASM_x_co_LI_D (const char * arg1, I_32 arg2);
extern J9_CFUNC I_32  ASM_x_co_LF_I (const char * arg1, float arg2);
extern J9_CFUNC I_32  ASM_x_co_LI_I (const char * arg1, I_32 arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_IFIF_J (I_32 arg1, float arg2, I_32 arg3, float arg4);
extern J9_CFUNC float  JNICALL C_x_co_IJIJ_F (I_32 arg1, I_64 arg2, I_32 arg3, I_64 arg4);
extern J9_CFUNC float  JNICALL C_x_co_ID_F (I_32 arg1, double arg2);
extern J9_CFUNC float  JNICALL C_x_co_IDID_F (I_32 arg1, double arg2, I_32 arg3, double arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_IF_J (I_32 arg1, float arg2);
extern J9_CFUNC float  ASM_x_co_LD_F (const char * arg1, double arg2);
extern J9_CFUNC float  JNICALL C_x_co_IJ_F (I_32 arg1, I_64 arg2);
extern J9_CFUNC double  JNICALL C_x_co_IL_D (I_32 arg1, const char * arg2);
extern J9_CFUNC float  JNICALL C_x_co_JD_F (I_64 arg1, double arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_IJIJ_J (I_32 arg1, I_64 arg2, I_32 arg3, I_64 arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_JF_J (I_64 arg1, float arg2);
extern J9_CFUNC double  JNICALL C_x_co_JL_D (I_64 arg1, const char * arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_ILIL_J (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC float  JNICALL C_x_co_JJ_F (I_64 arg1, I_64 arg2);
extern J9_CFUNC float  JNICALL C_co_FFFF_F (float arg1, float arg2, float arg3, float arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_IDID_J (I_32 arg1, double arg2, I_32 arg3, double arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_II_J (I_32 arg1, I_32 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_IFIF_L (I_32 arg1, float arg2, I_32 arg3, float arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_JI_J (I_64 arg1, I_32 arg2);
extern J9_CFUNC float  ASM_x_co_LI_F (const char * arg1, I_32 arg2);
extern J9_CFUNC float  ASM_x_co_LL_F (const char * arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_IIII_I (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC char*  JNICALL ASM_ci_LL_L (const char * arg1, const char * arg2);
extern J9_CFUNC char*  ASM_x_co_LJ_L (const char * arg1, I_64 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_IJ_L (I_32 arg1, I_64 arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_IIII_J (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC I_64  ASM_x_co_LI_J (const char * arg1, I_32 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_ILIL_L (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC double  JNICALL C_x_co_IJ_D (I_32 arg1, I_64 arg2);
extern J9_CFUNC double  JNICALL C_x_co_IFIF_D (I_32 arg1, float arg2, I_32 arg3, float arg4);
extern J9_CFUNC char*  JNICALL C_x_co_IL_L (I_32 arg1, const char * arg2);
extern J9_CFUNC char*  JNICALL C_x_co_JJ_L (I_64 arg1, I_64 arg2);
extern J9_CFUNC float  JNICALL C_x_co_IFIF_F (I_32 arg1, float arg2, I_32 arg3, float arg4);
extern J9_CFUNC double  ASM_x_co_LF_D (const char * arg1, float arg2);
extern J9_CFUNC char*  JNICALL C_x_co_IDID_L (I_32 arg1, double arg2, I_32 arg3, double arg4);
extern J9_CFUNC char*  JNICALL C_x_co_ID_L (I_32 arg1, double arg2);
extern J9_CFUNC float  JNICALL C_x_co_ILIL_F (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC double  JNICALL C_x_co_JJ_D (I_64 arg1, I_64 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_JL_L (I_64 arg1, const char * arg2);
extern J9_CFUNC I_32  ASM_x_co_LD_I (const char * arg1, double arg2);
extern J9_CFUNC char*  JNICALL C_x_co_JD_L (I_64 arg1, double arg2);
extern J9_CFUNC float  JNICALL C_x_co_IIII_F (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_IL_I (I_32 arg1, const char * arg2);
extern J9_CFUNC double  JNICALL C_x_co_IJIJ_D (I_32 arg1, I_64 arg2, I_32 arg3, I_64 arg4);
extern J9_CFUNC I_32  JNICALL C_ci_I (void);
extern J9_CFUNC char*  JNICALL C_x_co_IJIJ_L (I_32 arg1, I_64 arg2, I_32 arg3, I_64 arg4);
extern J9_CFUNC I_32  JNICALL C_co_IIIII_I (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4, I_32 arg5);
extern J9_CFUNC I_32  JNICALL C_x_co_JL_I (I_64 arg1, const char * arg2);
extern J9_CFUNC float  JNICALL C_x_co_IF_F (I_32 arg1, float arg2);
extern J9_CFUNC float  ASM_x_co_LJ_F (const char * arg1, I_64 arg2);
extern J9_CFUNC I_64  ASM_x_co_LL_J (const char * arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_IJIJ_I (I_32 arg1, I_64 arg2, I_32 arg3, I_64 arg4);
extern J9_CFUNC float  JNICALL C_x_co_JF_F (I_64 arg1, float arg2);
extern J9_CFUNC float  ASM_x_co_LF_F (const char * arg1, float arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_IL_J (I_32 arg1, const char * arg2);
extern J9_CFUNC double  ASM_x_co_LD_D (const char * arg1, double arg2);
extern J9_CFUNC I_32  JNICALL C_co_I (void);
extern J9_CFUNC I_64  JNICALL C_x_co_JL_J (I_64 arg1, const char * arg2);
extern J9_CFUNC double  ASM_x_co_LL_D (const char * arg1, const char * arg2);
extern J9_CFUNC char*  ASM_x_co_LF_L (const char * arg1, float arg2);
extern J9_CFUNC char*  ASM_x_co_LI_L (const char * arg1, I_32 arg2);
extern J9_CFUNC char*  ASM_x_co_LL_L (const char * arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_IDID_I (I_32 arg1, double arg2, I_32 arg3, double arg4);
extern J9_CFUNC I_64  ASM_x_co_LJ_J (const char * arg1, I_64 arg2);
extern J9_CFUNC I_64  ASM_x_co_LF_J (const char * arg1, float arg2);
extern J9_CFUNC double  JNICALL C_x_co_II_D (I_32 arg1, I_32 arg2);
extern J9_CFUNC double  ASM_x_co_LJ_D (const char * arg1, I_64 arg2);
extern J9_CFUNC float  JNICALL C_x_co_IL_F (I_32 arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_ID_I (I_32 arg1, double arg2);
extern J9_CFUNC double  JNICALL C_x_co_JI_D (I_64 arg1, I_32 arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_IFIF_I (I_32 arg1, float arg2, I_32 arg3, float arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_ILIL_I (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC double  JNICALL C_x_co_IIII_D (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC I_32  ASM_x_co_LJ_I (const char * arg1, I_64 arg2);
extern J9_CFUNC double  JNICALL C_x_co_IF_D (I_32 arg1, float arg2);
extern J9_CFUNC float  JNICALL C_x_co_JL_F (I_64 arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_JD_I (I_64 arg1, double arg2);
extern J9_CFUNC double  JNICALL C_x_co_ID_D (I_32 arg1, double arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_IJ_I (I_32 arg1, I_64 arg2);
extern J9_CFUNC I_32  JNICALL ASM_ci_I (void);
extern J9_CFUNC double  JNICALL C_x_co_JF_D (I_64 arg1, float arg2);
extern J9_CFUNC char*  JNICALL C_x_co_IIII_L (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_IF_I (I_32 arg1, float arg2);
extern J9_CFUNC double  JNICALL C_x_co_JD_D (I_64 arg1, double arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_IJ_J (I_32 arg1, I_64 arg2);
extern J9_CFUNC I_32  ASM_x_co_LL_I (const char * arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_JJ_I (I_64 arg1, I_64 arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_JF_I (I_64 arg1, float arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_JJ_J (I_64 arg1, I_64 arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_ID_J (I_32 arg1, double arg2);
extern J9_CFUNC float  JNICALL C_x_co_II_F (I_32 arg1, I_32 arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_JD_J (I_64 arg1, double arg2);
extern J9_CFUNC float  JNICALL C_x_co_JI_F (I_64 arg1, I_32 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_II_L (I_32 arg1, I_32 arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_II_I (I_32 arg1, I_32 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_JI_L (I_64 arg1, I_32 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_IF_L (I_32 arg1, float arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_JI_I (I_64 arg1, I_32 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_JF_L (I_64 arg1, float arg2);
extern J9_CFUNC char*  JNICALL C_co_LLLL_L (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC I_32  ASM_co_III_I (I_32 arg1, I_32 arg2, I_32 arg3);
extern J9_CFUNC I_64  ASM_co_J (void);
extern J9_CFUNC double  JNICALL C_co_D (void);
extern J9_CFUNC double  JNICALL C_x_ci_LL_D (const char * arg1, const char * arg2);
extern J9_CFUNC I_64  JNICALL C_x_ci_LI_J (const char * arg1, I_32 arg2);
extern J9_CFUNC I_32  ASM_x_co_DIDI_I (double arg1, I_32 arg2, double arg3, I_32 arg4);
extern J9_CFUNC I_32  JNICALL C_ci_III_I (I_32 arg1, I_32 arg2, I_32 arg3);
extern J9_CFUNC double  ASM_x_co_DJDJ_D (double arg1, I_64 arg2, double arg3, I_64 arg4);
extern J9_CFUNC I_32  ASM_x_co_DDDD_I (double arg1, double arg2, double arg3, double arg4);
extern J9_CFUNC char*  ASM_x_co_DDDD_L (double arg1, double arg2, double arg3, double arg4);
extern J9_CFUNC I_32  ASM_x_co_DFDF_I (double arg1, float arg2, double arg3, float arg4);
extern J9_CFUNC char*  JNICALL C_x_ci_LL_L (const char * arg1, const char * arg2);
extern J9_CFUNC float  ASM_x_co_DLDL_F (double arg1, const char * arg2, double arg3, const char * arg4);
extern J9_CFUNC I_32  ASM_x_co_DLDL_I (double arg1, const char * arg2, double arg3, const char * arg4);
extern J9_CFUNC char*  ASM_x_co_DJDJ_L (double arg1, I_64 arg2, double arg3, I_64 arg4);
extern J9_CFUNC I_32  JNICALL C_x_ci_LL_I (const char * arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_ci_IIIII_I (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4, I_32 arg5);
extern J9_CFUNC I_64  JNICALL C_x_ci_LL_J (const char * arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_co_I_I (I_32 arg1);
extern J9_CFUNC I_32  ASM_co_I_I (I_32 arg1);
extern J9_CFUNC I_32  ASM_x_co_DJDJ_I (double arg1, I_64 arg2, double arg3, I_64 arg4);
extern J9_CFUNC char*  ASM_x_co_DLDL_L (double arg1, const char * arg2, double arg3, const char * arg4);
extern J9_CFUNC I_64  ASM_x_co_DIDI_J (double arg1, I_32 arg2, double arg3, I_32 arg4);
extern J9_CFUNC float  ASM_x_co_DIDI_F (double arg1, I_32 arg2, double arg3, I_32 arg4);
extern J9_CFUNC double  ASM_x_co_DFDF_D (double arg1, float arg2, double arg3, float arg4);
extern J9_CFUNC I_64  ASM_x_co_DFDF_J (double arg1, float arg2, double arg3, float arg4);
extern J9_CFUNC float  ASM_x_co_DDDD_F (double arg1, double arg2, double arg3, double arg4);
extern J9_CFUNC double  JNICALL C_x_ci_LI_D (const char * arg1, I_32 arg2);
extern J9_CFUNC I_64  ASM_x_co_DJDJ_J (double arg1, I_64 arg2, double arg3, I_64 arg4);
extern J9_CFUNC float  JNICALL C_x_ci_LL_F (const char * arg1, const char * arg2);
extern J9_CFUNC double  ASM_x_co_DLDL_D (double arg1, const char * arg2, double arg3, const char * arg4);
extern J9_CFUNC double  ASM_x_co_DIDI_D (double arg1, I_32 arg2, double arg3, I_32 arg4);
extern J9_CFUNC float  JNICALL C_ci_FFF_F (float arg1, float arg2, float arg3);
extern J9_CFUNC float  ASM_x_co_DJDJ_F (double arg1, I_64 arg2, double arg3, I_64 arg4);
extern J9_CFUNC char*  ASM_x_co_DFDF_L (double arg1, float arg2, double arg3, float arg4);
extern J9_CFUNC float  JNICALL C_x_ci_LI_F (const char * arg1, I_32 arg2);
extern J9_CFUNC I_64  ASM_x_co_DDDD_J (double arg1, double arg2, double arg3, double arg4);
extern J9_CFUNC float  ASM_x_co_DFDF_F (double arg1, float arg2, double arg3, float arg4);
extern J9_CFUNC I_32  JNICALL C_x_custom_co_FDIJ_I (float arg1, double arg2, I_32 arg3, I_64 arg4);
extern J9_CFUNC char*  ASM_x_co_DIDI_L (double arg1, I_32 arg2, double arg3, I_32 arg4);
extern J9_CFUNC char*  JNICALL C_x_ci_LI_L (const char * arg1, I_32 arg2);
extern J9_CFUNC I_32  JNICALL C_x_ci_LI_I (const char * arg1, I_32 arg2);
extern J9_CFUNC double  JNICALL C_x_co_JLJL_D (I_64 arg1, const char * arg2, I_64 arg3, const char * arg4);
extern J9_CFUNC double  ASM_x_co_DDDD_D (double arg1, double arg2, double arg3, double arg4);
extern J9_CFUNC char*  ASM_x_co_FD_L (float arg1, double arg2);
extern J9_CFUNC char*  JNICALL C_ci_LL_L (const char * arg1, const char * arg2);
extern J9_CFUNC char*  JNICALL C_co_LLL_L (const char * arg1, const char * arg2, const char * arg3);
extern J9_CFUNC I_64  ASM_x_co_FD_J (float arg1, double arg2);
extern J9_CFUNC double  ASM_x_co_FI_D (float arg1, I_32 arg2);
extern J9_CFUNC double  JNICALL C_x_co_JDJD_D (I_64 arg1, double arg2, I_64 arg3, double arg4);
extern J9_CFUNC I_64  ASM_x_co_DLDL_J (double arg1, const char * arg2, double arg3, const char * arg4);
extern J9_CFUNC I_32  ASM_x_co_FF_I (float arg1, float arg2);
extern J9_CFUNC I_32  ASM_x_co_FI_I (float arg1, I_32 arg2);
extern J9_CFUNC float  JNICALL C_x_co_DD_F (double arg1, double arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_DF_J (double arg1, float arg2);
extern J9_CFUNC float  ASM_x_co_FD_F (float arg1, double arg2);
extern J9_CFUNC float  JNICALL C_x_co_DJ_F (double arg1, I_64 arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_JFJF_J (I_64 arg1, float arg2, I_64 arg3, float arg4);
extern J9_CFUNC double  JNICALL ASM_x_ci_LI_D (const char * arg1, I_32 arg2);
extern J9_CFUNC double  JNICALL C_x_co_DL_D (double arg1, const char * arg2);
extern J9_CFUNC float  JNICALL C_x_co_JJJJ_F (I_64 arg1, I_64 arg2, I_64 arg3, I_64 arg4);
extern J9_CFUNC float  JNICALL C_x_co_JDJD_F (I_64 arg1, double arg2, I_64 arg3, double arg4);
extern J9_CFUNC I_32  JNICALL ASM_x_ci_LI_I (const char * arg1, I_32 arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_DI_J (double arg1, I_32 arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_JJJJ_J (I_64 arg1, I_64 arg2, I_64 arg3, I_64 arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_JLJL_J (I_64 arg1, const char * arg2, I_64 arg3, const char * arg4);
extern J9_CFUNC double  JNICALL ASM_x_ci_II_D (I_32 arg1, I_32 arg2);
extern J9_CFUNC double  JNICALL C_x_co_FLFL_D (float arg1, const char * arg2, float arg3, const char * arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_JDJD_J (I_64 arg1, double arg2, I_64 arg3, double arg4);
extern J9_CFUNC float  ASM_x_co_FI_F (float arg1, I_32 arg2);
extern J9_CFUNC float  ASM_x_co_FL_F (float arg1, const char * arg2);
extern J9_CFUNC char*  ASM_x_co_FJ_L (float arg1, I_64 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_JFJF_L (I_64 arg1, float arg2, I_64 arg3, float arg4);
extern J9_CFUNC I_32  JNICALL ASM_x_ci_II_I (I_32 arg1, I_32 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_DJ_L (double arg1, I_64 arg2);
extern J9_CFUNC I_64  ASM_x_co_FI_J (float arg1, I_32 arg2);
extern J9_CFUNC double  JNICALL C_x_co_FDFD_D (float arg1, double arg2, float arg3, double arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_JIJI_I (I_64 arg1, I_32 arg2, I_64 arg3, I_32 arg4);
extern J9_CFUNC double  JNICALL C_x_co_DJ_D (double arg1, I_64 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_DL_L (double arg1, const char * arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_JIJI_J (I_64 arg1, I_32 arg2, I_64 arg3, I_32 arg4);
extern J9_CFUNC float  JNICALL ASM_x_ci_LI_F (const char * arg1, I_32 arg2);
extern J9_CFUNC double  ASM_x_co_FF_D (float arg1, float arg2);
extern J9_CFUNC char*  JNICALL C_x_co_JLJL_L (I_64 arg1, const char * arg2, I_64 arg3, const char * arg4);
extern J9_CFUNC float  JNICALL ASM_x_ci_LL_F (const char * arg1, const char * arg2);
extern J9_CFUNC char*  JNICALL C_x_co_DD_L (double arg1, double arg2);
extern J9_CFUNC double  JNICALL C_x_co_JFJF_D (I_64 arg1, float arg2, I_64 arg3, float arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_FFFF_J (float arg1, float arg2, float arg3, float arg4);
extern J9_CFUNC I_32  ASM_x_co_FD_I (float arg1, double arg2);
extern J9_CFUNC float  JNICALL C_x_co_FJFJ_F (float arg1, I_64 arg2, float arg3, I_64 arg4);
extern J9_CFUNC I_64  JNICALL ASM_x_ci_LI_J (const char * arg1, I_32 arg2);
extern J9_CFUNC float  JNICALL C_x_co_JFJF_F (I_64 arg1, float arg2, I_64 arg3, float arg4);
extern J9_CFUNC char*  JNICALL C_x_co_JDJD_L (I_64 arg1, double arg2, I_64 arg3, double arg4);
extern J9_CFUNC float  JNICALL C_x_co_FDFD_F (float arg1, double arg2, float arg3, double arg4);
extern J9_CFUNC float  JNICALL C_x_co_JLJL_F (I_64 arg1, const char * arg2, I_64 arg3, const char * arg4);
extern J9_CFUNC float  JNICALL ASM_x_ci_II_F (I_32 arg1, I_32 arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_FJFJ_J (float arg1, I_64 arg2, float arg3, I_64 arg4);
extern J9_CFUNC float  JNICALL ASM_x_ci_IL_F (I_32 arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_DL_I (double arg1, const char * arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_FLFL_J (float arg1, const char * arg2, float arg3, const char * arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_FDFD_J (float arg1, double arg2, float arg3, double arg4);
extern J9_CFUNC I_64  JNICALL ASM_x_ci_II_J (I_32 arg1, I_32 arg2);
extern J9_CFUNC float  JNICALL C_x_co_JIJI_F (I_64 arg1, I_32 arg2, I_64 arg3, I_32 arg4);
extern J9_CFUNC float  JNICALL C_x_co_DF_F (double arg1, float arg2);
extern J9_CFUNC double  JNICALL C_x_co_JJJJ_D (I_64 arg1, I_64 arg2, I_64 arg3, I_64 arg4);
extern J9_CFUNC char*  JNICALL C_x_co_FFFF_L (float arg1, float arg2, float arg3, float arg4);
extern J9_CFUNC float  ASM_x_co_FJ_F (float arg1, I_64 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_JJJJ_L (I_64 arg1, I_64 arg2, I_64 arg3, I_64 arg4);
extern J9_CFUNC I_64  ASM_x_co_FL_J (float arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_FIFI_I (float arg1, I_32 arg2, float arg3, I_32 arg4);
extern J9_CFUNC float  JNICALL C_ci_F_F (float arg1);
extern J9_CFUNC I_64  JNICALL C_x_co_FIFI_J (float arg1, I_32 arg2, float arg3, I_32 arg4);
extern J9_CFUNC char*  JNICALL C_x_co_FLFL_L (float arg1, const char * arg2, float arg3, const char * arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_JJJJ_I (I_64 arg1, I_64 arg2, I_64 arg3, I_64 arg4);
extern J9_CFUNC float  ASM_x_co_FF_F (float arg1, float arg2);
extern J9_CFUNC double  JNICALL C_x_co_FFFF_D (float arg1, float arg2, float arg3, float arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_DL_J (double arg1, const char * arg2);
extern J9_CFUNC double  ASM_x_co_FD_D (float arg1, double arg2);
extern J9_CFUNC float  JNICALL C_x_co_FFFF_F (float arg1, float arg2, float arg3, float arg4);
extern J9_CFUNC I_64  JNICALL ASM_x_ci_LL_J (const char * arg1, const char * arg2);
extern J9_CFUNC char*  JNICALL C_x_co_FDFD_L (float arg1, double arg2, float arg3, double arg4);
extern J9_CFUNC float  JNICALL C_x_co_FLFL_F (float arg1, const char * arg2, float arg3, const char * arg4);
extern J9_CFUNC I_64  JNICALL C_ci_J_J (I_64 arg1);
extern J9_CFUNC double  ASM_x_co_FL_D (float arg1, const char * arg2);
extern J9_CFUNC char*  ASM_x_co_FF_L (float arg1, float arg2);
extern J9_CFUNC char*  ASM_x_co_FI_L (float arg1, I_32 arg2);
extern J9_CFUNC char*  ASM_x_co_FL_L (float arg1, const char * arg2);
extern J9_CFUNC I_64  JNICALL ASM_x_ci_IL_J (I_32 arg1, const char * arg2);
extern J9_CFUNC I_64  ASM_x_co_FJ_J (float arg1, I_64 arg2);
extern J9_CFUNC I_64  ASM_x_co_FF_J (float arg1, float arg2);
extern J9_CFUNC float  JNICALL C_x_co_FIFI_F (float arg1, I_32 arg2, float arg3, I_32 arg4);
extern J9_CFUNC double  ASM_x_co_FJ_D (float arg1, I_64 arg2);
extern J9_CFUNC double  JNICALL C_x_co_FJFJ_D (float arg1, I_64 arg2, float arg3, I_64 arg4);
extern J9_CFUNC double  JNICALL C_x_co_DI_D (double arg1, I_32 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_FJFJ_L (float arg1, I_64 arg2, float arg3, I_64 arg4);
extern J9_CFUNC float  JNICALL C_x_co_DL_F (double arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_DD_I (double arg1, double arg2);
extern J9_CFUNC double  JNICALL ASM_x_ci_LL_D (const char * arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_JDJD_I (I_64 arg1, double arg2, I_64 arg3, double arg4);
extern J9_CFUNC double  JNICALL C_x_co_DF_D (double arg1, float arg2);
extern J9_CFUNC I_32  ASM_x_co_FJ_I (float arg1, I_64 arg2);
extern J9_CFUNC char*  JNICALL ASM_x_ci_LI_L (const char * arg1, I_32 arg2);
extern J9_CFUNC float  JNICALL C_ci_F (void);
extern J9_CFUNC char*  JNICALL ASM_x_ci_LL_L (const char * arg1, const char * arg2);
extern J9_CFUNC double  JNICALL C_x_co_DD_D (double arg1, double arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_DJ_I (double arg1, I_64 arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_JFJF_I (I_64 arg1, float arg2, I_64 arg3, float arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_JLJL_I (I_64 arg1, const char * arg2, I_64 arg3, const char * arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_FJFJ_I (float arg1, I_64 arg2, float arg3, I_64 arg4);
extern J9_CFUNC I_32  ASM_co_I (void);
extern J9_CFUNC double  JNICALL C_x_co_JIJI_D (I_64 arg1, I_32 arg2, I_64 arg3, I_32 arg4);
extern J9_CFUNC double  JNICALL ASM_x_ci_IL_D (I_32 arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_DF_I (double arg1, float arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_DJ_J (double arg1, I_64 arg2);
extern J9_CFUNC I_32  ASM_x_co_FL_I (float arg1, const char * arg2);
extern J9_CFUNC char*  JNICALL ASM_x_ci_II_L (I_32 arg1, I_32 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_JIJI_L (I_64 arg1, I_32 arg2, I_64 arg3, I_32 arg4);
extern J9_CFUNC char*  JNICALL ASM_x_ci_IL_L (I_32 arg1, const char * arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_DD_J (double arg1, double arg2);
extern J9_CFUNC char*  ASM_co_LLLLL_L (const char * arg1, const char * arg2, const char * arg3, const char * arg4, const char * arg5);
extern J9_CFUNC float  JNICALL C_x_co_DI_F (double arg1, I_32 arg2);
extern J9_CFUNC I_32  JNICALL ASM_x_ci_LL_I (const char * arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_FDFD_I (float arg1, double arg2, float arg3, double arg4);
extern J9_CFUNC char*  JNICALL C_x_co_DI_L (double arg1, I_32 arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_DI_I (double arg1, I_32 arg2);
extern J9_CFUNC I_32  JNICALL ASM_x_ci_IL_I (I_32 arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL ASM_x_ci_LILI_I (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_FLFL_I (float arg1, const char * arg2, float arg3, const char * arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_FFFF_I (float arg1, float arg2, float arg3, float arg4);
extern J9_CFUNC char*  JNICALL C_x_co_DF_L (double arg1, float arg2);
extern J9_CFUNC double  JNICALL C_x_co_FIFI_D (float arg1, I_32 arg2, float arg3, I_32 arg4);
extern J9_CFUNC I_32  ASM_x_custom_co_FDIJ_I (float arg1, double arg2, I_32 arg3, I_64 arg4);
extern J9_CFUNC double  JNICALL ASM_ci_DD_D (double arg1, double arg2);
extern J9_CFUNC char*  JNICALL C_x_co_FIFI_L (float arg1, I_32 arg2, float arg3, I_32 arg4);
extern J9_CFUNC I_32  ASM_co_IIII_I (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC float  JNICALL ASM_ci_FFFFF_F (float arg1, float arg2, float arg3, float arg4, float arg5);
extern J9_CFUNC float  JNICALL ASM_x_ci_LLLL_F (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC I_32  JNICALL ASM_x_ci_LLLL_I (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC float  ASM_co_FFFFF_F (float arg1, float arg2, float arg3, float arg4, float arg5);
extern J9_CFUNC I_32  JNICALL ASM_ci_III_I (I_32 arg1, I_32 arg2, I_32 arg3);
extern J9_CFUNC float  JNICALL ASM_x_ci_LILI_F (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC char*  JNICALL ASM_x_ci_LLLL_L (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC I_64  JNICALL ASM_x_ci_LILI_J (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC char*  JNICALL ASM_ci_L_L (const char * arg1);
extern J9_CFUNC double  JNICALL ASM_x_ci_LLLL_D (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC double  JNICALL ASM_x_ci_LILI_D (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC float  JNICALL C_ci_FFFFF_F (float arg1, float arg2, float arg3, float arg4, float arg5);
extern J9_CFUNC I_64  JNICALL C_co_JJ_J (I_64 arg1, I_64 arg2);
extern J9_CFUNC I_32  JNICALL ASM_ci_I_I (I_32 arg1);
extern J9_CFUNC double  ASM_co_D_D (double arg1);
extern J9_CFUNC char*  JNICALL ASM_x_ci_LILI_L (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC I_64  JNICALL ASM_x_ci_LLLL_J (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC double  JNICALL C_x_co_LLLL_D (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC double  JNICALL C_x_co_LDLD_D (const char * arg1, double arg2, const char * arg3, double arg4);
extern J9_CFUNC I_32  ASM_x_co_FIFI_I (float arg1, I_32 arg2, float arg3, I_32 arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_LFLF_J (const char * arg1, float arg2, const char * arg3, float arg4);
extern J9_CFUNC float  JNICALL C_x_co_LJLJ_F (const char * arg1, I_64 arg2, const char * arg3, I_64 arg4);
extern J9_CFUNC float  JNICALL C_x_co_LDLD_F (const char * arg1, double arg2, const char * arg3, double arg4);
extern J9_CFUNC double  ASM_x_co_FJFJ_D (float arg1, I_64 arg2, float arg3, I_64 arg4);
extern J9_CFUNC I_32  ASM_x_co_FDFD_I (float arg1, double arg2, float arg3, double arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_LJLJ_J (const char * arg1, I_64 arg2, const char * arg3, I_64 arg4);
extern J9_CFUNC char*  ASM_x_co_FDFD_L (float arg1, double arg2, float arg3, double arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_LLLL_J (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC I_32  ASM_x_co_FFFF_I (float arg1, float arg2, float arg3, float arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_LDLD_J (const char * arg1, double arg2, const char * arg3, double arg4);
extern J9_CFUNC char*  JNICALL C_ci_LLL_L (const char * arg1, const char * arg2, const char * arg3);
extern J9_CFUNC char*  JNICALL C_x_co_LFLF_L (const char * arg1, float arg2, const char * arg3, float arg4);
extern J9_CFUNC float  ASM_x_co_FLFL_F (float arg1, const char * arg2, float arg3, const char * arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_LILI_I (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_LILI_J (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC I_32  ASM_x_co_FLFL_I (float arg1, const char * arg2, float arg3, const char * arg4);
extern J9_CFUNC char*  JNICALL C_x_co_LLLL_L (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC char*  ASM_x_co_FJFJ_L (float arg1, I_64 arg2, float arg3, I_64 arg4);
extern J9_CFUNC double  JNICALL C_x_co_LFLF_D (const char * arg1, float arg2, const char * arg3, float arg4);
extern J9_CFUNC float  JNICALL C_x_co_LFLF_F (const char * arg1, float arg2, const char * arg3, float arg4);
extern J9_CFUNC char*  JNICALL C_x_co_LDLD_L (const char * arg1, double arg2, const char * arg3, double arg4);
extern J9_CFUNC I_32  JNICALL ASM_ci_IIII_I (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC float  JNICALL C_x_co_LLLL_F (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC float  JNICALL C_ci_FFFF_F (float arg1, float arg2, float arg3, float arg4);
extern J9_CFUNC char*  ASM_co_LL_L (const char * arg1, const char * arg2);
extern J9_CFUNC float  JNICALL C_co_FFFFF_F (float arg1, float arg2, float arg3, float arg4, float arg5);
extern J9_CFUNC float  JNICALL C_x_co_LILI_F (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC double  JNICALL C_x_co_LJLJ_D (const char * arg1, I_64 arg2, const char * arg3, I_64 arg4);
extern J9_CFUNC char*  JNICALL C_x_co_LJLJ_L (const char * arg1, I_64 arg2, const char * arg3, I_64 arg4);
extern J9_CFUNC float  ASM_x_co_FIFI_F (float arg1, I_32 arg2, float arg3, I_32 arg4);
extern J9_CFUNC I_32  ASM_x_co_FJFJ_I (float arg1, I_64 arg2, float arg3, I_64 arg4);
extern J9_CFUNC char*  ASM_x_co_FLFL_L (float arg1, const char * arg2, float arg3, const char * arg4);
extern J9_CFUNC I_64  ASM_x_co_FIFI_J (float arg1, I_32 arg2, float arg3, I_32 arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_LJLJ_I (const char * arg1, I_64 arg2, const char * arg3, I_64 arg4);
extern J9_CFUNC I_64  ASM_co_J_J (I_64 arg1);
extern J9_CFUNC double  ASM_x_co_FFFF_D (float arg1, float arg2, float arg3, float arg4);
extern J9_CFUNC void  asmFailure (char *reason);
extern J9_CFUNC I_64  ASM_x_co_FFFF_J (float arg1, float arg2, float arg3, float arg4);
extern J9_CFUNC float  ASM_x_co_FDFD_F (float arg1, double arg2, float arg3, double arg4);
extern J9_CFUNC I_64  ASM_x_co_FJFJ_J (float arg1, I_64 arg2, float arg3, I_64 arg4);
extern J9_CFUNC char*  JNICALL C_co_L (void);
extern J9_CFUNC double  ASM_x_co_FLFL_D (float arg1, const char * arg2, float arg3, const char * arg4);
extern J9_CFUNC double  ASM_x_co_FIFI_D (float arg1, I_32 arg2, float arg3, I_32 arg4);
extern J9_CFUNC float  ASM_x_co_FJFJ_F (float arg1, I_64 arg2, float arg3, I_64 arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_LDLD_I (const char * arg1, double arg2, const char * arg3, double arg4);
extern J9_CFUNC char*  ASM_x_co_FFFF_L (float arg1, float arg2, float arg3, float arg4);
extern J9_CFUNC char*  JNICALL C_ci_L_L (const char * arg1);
extern J9_CFUNC I_64  ASM_co_JJ_J (I_64 arg1, I_64 arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_LFLF_I (const char * arg1, float arg2, const char * arg3, float arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_LLLL_I (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC double  JNICALL C_x_co_LILI_D (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC float  ASM_co_FFF_F (float arg1, float arg2, float arg3);
extern J9_CFUNC I_64  ASM_x_co_FDFD_J (float arg1, double arg2, float arg3, double arg4);
extern J9_CFUNC float  ASM_x_co_FFFF_F (float arg1, float arg2, float arg3, float arg4);
extern J9_CFUNC char*  JNICALL C_x_co_LILI_L (const char * arg1, I_32 arg2, const char * arg3, I_32 arg4);
extern J9_CFUNC char*  ASM_x_co_FIFI_L (float arg1, I_32 arg2, float arg3, I_32 arg4);
extern J9_CFUNC double  ASM_x_co_FDFD_D (float arg1, double arg2, float arg3, double arg4);
extern J9_CFUNC I_64  ASM_x_co_FLFL_J (float arg1, const char * arg2, float arg3, const char * arg4);
extern J9_CFUNC I_32  JNICALL ASM_x_ci_IIII_I (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC I_32  ASM_co_IIIII_I (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4, I_32 arg5);
extern J9_CFUNC double  JNICALL C_co_DD_D (double arg1, double arg2);
extern J9_CFUNC float  JNICALL ASM_x_ci_ILIL_F (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC I_32  JNICALL ASM_x_ci_ILIL_I (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC char*  ASM_co_L_L (const char * arg1);
extern J9_CFUNC float  JNICALL ASM_x_ci_IIII_F (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC char*  JNICALL ASM_x_ci_ILIL_L (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC I_64  JNICALL ASM_x_ci_IIII_J (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC double  JNICALL ASM_x_ci_ILIL_D (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC double  JNICALL ASM_x_ci_IIII_D (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC char*  JNICALL ASM_x_ci_IIII_L (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC I_64  JNICALL ASM_x_ci_ILIL_J (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC I_32  JNICALL ASM_ci_II_I (I_32 arg1, I_32 arg2);
extern J9_CFUNC double  JNICALL C_x_co_DLDL_D (double arg1, const char * arg2, double arg3, const char * arg4);
extern J9_CFUNC I_32  JNICALL C_co_III_I (I_32 arg1, I_32 arg2, I_32 arg3);
extern J9_CFUNC double  JNICALL C_x_co_DDDD_D (double arg1, double arg2, double arg3, double arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_DFDF_J (double arg1, float arg2, double arg3, float arg4);
extern J9_CFUNC float  JNICALL C_x_co_DJDJ_F (double arg1, I_64 arg2, double arg3, I_64 arg4);
extern J9_CFUNC float  JNICALL C_x_co_DDDD_F (double arg1, double arg2, double arg3, double arg4);
extern J9_CFUNC float  JNICALL ASM_ci_FF_F (float arg1, float arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_DJDJ_J (double arg1, I_64 arg2, double arg3, I_64 arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_DLDL_J (double arg1, const char * arg2, double arg3, const char * arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_DDDD_J (double arg1, double arg2, double arg3, double arg4);
extern J9_CFUNC I_64  ASM_x_custom_co_FDFDFDFDFD_J (float arg1, double arg2, float arg3, double arg4, float arg5, double arg6, float arg7, double arg8, float arg9, double arg10);
extern J9_CFUNC char*  JNICALL C_x_co_DFDF_L (double arg1, float arg2, double arg3, float arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_DIDI_I (double arg1, I_32 arg2, double arg3, I_32 arg4);
extern J9_CFUNC float  JNICALL C_co_F (void);
extern J9_CFUNC I_64  JNICALL C_x_co_DIDI_J (double arg1, I_32 arg2, double arg3, I_32 arg4);
extern J9_CFUNC char*  JNICALL C_x_co_DLDL_L (double arg1, const char * arg2, double arg3, const char * arg4);
extern J9_CFUNC I_64  JNICALL C_co_J_J (I_64 arg1);
extern J9_CFUNC double  JNICALL C_x_co_DFDF_D (double arg1, float arg2, double arg3, float arg4);
extern J9_CFUNC float  JNICALL ASM_ci_FFFF_F (float arg1, float arg2, float arg3, float arg4);
extern J9_CFUNC float  JNICALL C_x_co_DFDF_F (double arg1, float arg2, double arg3, float arg4);
extern J9_CFUNC char*  JNICALL C_x_co_DDDD_L (double arg1, double arg2, double arg3, double arg4);
extern J9_CFUNC float  JNICALL C_x_co_DLDL_F (double arg1, const char * arg2, double arg3, const char * arg4);
extern J9_CFUNC char*  JNICALL C_ci_L (void);
extern J9_CFUNC float  ASM_co_FFFF_F (float arg1, float arg2, float arg3, float arg4);
extern J9_CFUNC float  JNICALL C_x_co_DIDI_F (double arg1, I_32 arg2, double arg3, I_32 arg4);
extern J9_CFUNC double  JNICALL C_x_co_DJDJ_D (double arg1, I_64 arg2, double arg3, I_64 arg4);
extern J9_CFUNC char*  JNICALL C_x_co_DJDJ_L (double arg1, I_64 arg2, double arg3, I_64 arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_DJDJ_I (double arg1, I_64 arg2, double arg3, I_64 arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_DDDD_I (double arg1, double arg2, double arg3, double arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_DFDF_I (double arg1, float arg2, double arg3, float arg4);
extern J9_CFUNC I_32  JNICALL C_x_co_DLDL_I (double arg1, const char * arg2, double arg3, const char * arg4);
extern J9_CFUNC double  JNICALL C_x_co_DIDI_D (double arg1, I_32 arg2, double arg3, I_32 arg4);
extern J9_CFUNC I_32  JNICALL ASM_ci_IIIII_I (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4, I_32 arg5);
extern J9_CFUNC char*  JNICALL C_x_co_DIDI_L (double arg1, I_32 arg2, double arg3, I_32 arg4);
extern J9_CFUNC char*  ASM_x_co_JD_L (I_64 arg1, double arg2);
extern J9_CFUNC double  ASM_x_co_JI_D (I_64 arg1, I_32 arg2);
extern J9_CFUNC I_64  ASM_x_co_JD_J (I_64 arg1, double arg2);
extern J9_CFUNC I_32  ASM_x_co_JF_I (I_64 arg1, float arg2);
extern J9_CFUNC I_32  ASM_x_co_JI_I (I_64 arg1, I_32 arg2);
extern J9_CFUNC float  ASM_x_co_JD_F (I_64 arg1, double arg2);
extern J9_CFUNC float  JNICALL C_co_FF_F (float arg1, float arg2);
extern J9_CFUNC char*  ASM_x_co_ID_L (I_32 arg1, double arg2);
extern J9_CFUNC double  ASM_x_co_II_D (I_32 arg1, I_32 arg2);
extern J9_CFUNC I_64  ASM_x_co_ID_J (I_32 arg1, double arg2);
extern J9_CFUNC I_32  ASM_x_co_IF_I (I_32 arg1, float arg2);
extern J9_CFUNC I_32  ASM_x_co_II_I (I_32 arg1, I_32 arg2);
extern J9_CFUNC float  ASM_x_co_JI_F (I_64 arg1, I_32 arg2);
extern J9_CFUNC float  ASM_x_co_JL_F (I_64 arg1, const char * arg2);
extern J9_CFUNC float  ASM_x_co_ID_F (I_32 arg1, double arg2);
extern J9_CFUNC char*  ASM_x_co_JJ_L (I_64 arg1, I_64 arg2);
extern J9_CFUNC char*  JNICALL ASM_ci_LLLL_L (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC I_64  ASM_x_co_JI_J (I_64 arg1, I_32 arg2);
extern J9_CFUNC double  ASM_x_co_JF_D (I_64 arg1, float arg2);
extern J9_CFUNC I_32  ASM_x_co_JD_I (I_64 arg1, double arg2);
extern J9_CFUNC float  ASM_x_co_II_F (I_32 arg1, I_32 arg2);
extern J9_CFUNC float  ASM_x_co_IL_F (I_32 arg1, const char * arg2);
extern J9_CFUNC char*  ASM_x_co_IJ_L (I_32 arg1, I_64 arg2);
extern J9_CFUNC char*  ASM_co_LLLL_L (const char * arg1, const char * arg2, const char * arg3, const char * arg4);
extern J9_CFUNC I_64  ASM_x_co_II_J (I_32 arg1, I_32 arg2);
extern J9_CFUNC double  ASM_x_co_IF_D (I_32 arg1, float arg2);
extern J9_CFUNC float  ASM_x_co_JJ_F (I_64 arg1, I_64 arg2);
extern J9_CFUNC I_32  ASM_x_co_ID_I (I_32 arg1, double arg2);
extern J9_CFUNC I_64  ASM_x_co_JL_J (I_64 arg1, const char * arg2);
extern J9_CFUNC double  ASM_co_D (void);
extern J9_CFUNC float  ASM_x_co_JF_F (I_64 arg1, float arg2);
extern J9_CFUNC double  ASM_x_co_JD_D (I_64 arg1, double arg2);
extern J9_CFUNC float  ASM_x_co_IJ_F (I_32 arg1, I_64 arg2);
extern J9_CFUNC double  ASM_x_co_JL_D (I_64 arg1, const char * arg2);
extern J9_CFUNC I_64  ASM_x_co_IL_J (I_32 arg1, const char * arg2);
extern J9_CFUNC char*  ASM_x_co_JF_L (I_64 arg1, float arg2);
extern J9_CFUNC char*  ASM_x_co_JI_L (I_64 arg1, I_32 arg2);
extern J9_CFUNC char*  ASM_x_co_JL_L (I_64 arg1, const char * arg2);
extern J9_CFUNC I_64  ASM_x_co_JJ_J (I_64 arg1, I_64 arg2);
extern J9_CFUNC I_64  ASM_x_co_JF_J (I_64 arg1, float arg2);
extern J9_CFUNC float  ASM_x_co_IF_F (I_32 arg1, float arg2);
extern J9_CFUNC double  ASM_x_co_JJ_D (I_64 arg1, I_64 arg2);
extern J9_CFUNC double  ASM_x_co_ID_D (I_32 arg1, double arg2);
extern J9_CFUNC float  JNICALL ASM_ci_F (void);
extern J9_CFUNC I_32  ASM_x_co_JJ_I (I_64 arg1, I_64 arg2);
extern J9_CFUNC double  ASM_x_co_IL_D (I_32 arg1, const char * arg2);
extern J9_CFUNC char*  ASM_x_co_IF_L (I_32 arg1, float arg2);
extern J9_CFUNC char*  ASM_x_co_II_L (I_32 arg1, I_32 arg2);
extern J9_CFUNC char*  ASM_x_co_IL_L (I_32 arg1, const char * arg2);
extern J9_CFUNC I_32  ASM_x_co_JL_I (I_64 arg1, const char * arg2);
extern J9_CFUNC I_64  ASM_x_co_IJ_J (I_32 arg1, I_64 arg2);
extern J9_CFUNC I_64  ASM_x_co_IF_J (I_32 arg1, float arg2);
extern J9_CFUNC double  ASM_x_co_IJ_D (I_32 arg1, I_64 arg2);
extern J9_CFUNC I_32  ASM_x_co_IJ_I (I_32 arg1, I_64 arg2);
extern J9_CFUNC I_32  ASM_x_co_IL_I (I_32 arg1, const char * arg2);
extern J9_CFUNC double  JNICALL C_x_ci_IL_D (I_32 arg1, const char * arg2);
extern J9_CFUNC double  JNICALL C_x_ci_ILIL_D (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC char*  ASM_x_co_DD_L (double arg1, double arg2);
extern J9_CFUNC double  ASM_x_co_DI_D (double arg1, I_32 arg2);
extern J9_CFUNC I_64  ASM_x_co_DD_J (double arg1, double arg2);
extern J9_CFUNC I_64  JNICALL C_x_ci_II_J (I_32 arg1, I_32 arg2);
extern J9_CFUNC I_32  ASM_x_co_DF_I (double arg1, float arg2);
extern J9_CFUNC I_32  ASM_x_co_DI_I (double arg1, I_32 arg2);
extern J9_CFUNC float  ASM_x_co_DD_F (double arg1, double arg2);
extern J9_CFUNC char*  JNICALL C_x_ci_IL_L (I_32 arg1, const char * arg2);
extern J9_CFUNC char*  JNICALL ASM_ci_LLL_L (const char * arg1, const char * arg2, const char * arg3);
extern J9_CFUNC I_64  JNICALL C_x_ci_ILIL_J (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC float  ASM_x_co_DI_F (double arg1, I_32 arg2);
extern J9_CFUNC float  ASM_x_co_DL_F (double arg1, const char * arg2);
extern J9_CFUNC char*  ASM_x_co_DJ_L (double arg1, I_64 arg2);
extern J9_CFUNC I_64  ASM_x_co_DI_J (double arg1, I_32 arg2);
extern J9_CFUNC I_32  JNICALL C_x_ci_IIII_I (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC I_32  JNICALL C_x_ci_IL_I (I_32 arg1, const char * arg2);
extern J9_CFUNC I_64  JNICALL C_x_ci_IIII_J (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC double  ASM_x_co_DF_D (double arg1, float arg2);
extern J9_CFUNC char*  JNICALL C_x_ci_ILIL_L (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC I_32  ASM_x_co_DD_I (double arg1, double arg2);
extern J9_CFUNC float  JNICALL C_x_ci_ILIL_F (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC I_64  JNICALL C_x_ci_IL_J (I_32 arg1, const char * arg2);
extern J9_CFUNC float  JNICALL C_x_ci_IIII_F (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC float  ASM_x_co_DJ_F (double arg1, I_64 arg2);
extern J9_CFUNC I_64  ASM_x_co_DL_J (double arg1, const char * arg2);
extern J9_CFUNC double  JNICALL ASM_ci_D (void);
extern J9_CFUNC double  JNICALL C_x_ci_II_D (I_32 arg1, I_32 arg2);
extern J9_CFUNC float  ASM_x_co_DF_F (double arg1, float arg2);
extern J9_CFUNC float  JNICALL C_x_ci_IL_F (I_32 arg1, const char * arg2);
extern J9_CFUNC double  ASM_x_co_DD_D (double arg1, double arg2);
extern J9_CFUNC double  ASM_x_co_DL_D (double arg1, const char * arg2);
extern J9_CFUNC char*  ASM_x_co_DF_L (double arg1, float arg2);
extern J9_CFUNC char*  ASM_x_co_DI_L (double arg1, I_32 arg2);
extern J9_CFUNC I_64  JNICALL C_co_J (void);
extern J9_CFUNC char*  ASM_x_co_DL_L (double arg1, const char * arg2);
extern J9_CFUNC I_64  ASM_x_co_DJ_J (double arg1, I_64 arg2);
extern J9_CFUNC I_64  ASM_x_co_DF_J (double arg1, float arg2);
extern J9_CFUNC double  ASM_x_co_DJ_D (double arg1, I_64 arg2);
extern J9_CFUNC char*  JNICALL C_co_LLLLL_L (const char * arg1, const char * arg2, const char * arg3, const char * arg4, const char * arg5);
extern J9_CFUNC I_32  ASM_x_co_DJ_I (double arg1, I_64 arg2);
extern J9_CFUNC char*  JNICALL C_ci_LLLLL_L (const char * arg1, const char * arg2, const char * arg3, const char * arg4, const char * arg5);
extern J9_CFUNC float  JNICALL C_x_ci_II_F (I_32 arg1, I_32 arg2);
extern J9_CFUNC float  JNICALL C_x_co_LD_F (const char * arg1, double arg2);
extern J9_CFUNC I_32  JNICALL C_x_ci_ILIL_I (I_32 arg1, const char * arg2, I_32 arg3, const char * arg4);
extern J9_CFUNC double  JNICALL C_x_ci_IIII_D (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC I_64  JNICALL C_x_co_LF_J (const char * arg1, float arg2);
extern J9_CFUNC double  JNICALL C_x_co_LL_D (const char * arg1, const char * arg2);
extern J9_CFUNC float  JNICALL C_x_co_LJ_F (const char * arg1, I_64 arg2);
extern J9_CFUNC I_32  ASM_x_co_DL_I (double arg1, const char * arg2);
extern J9_CFUNC char*  JNICALL C_x_ci_II_L (I_32 arg1, I_32 arg2);
extern J9_CFUNC char*  JNICALL C_x_ci_IIII_L (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4);
extern J9_CFUNC I_32  JNICALL C_x_ci_II_I (I_32 arg1, I_32 arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_LI_J (const char * arg1, I_32 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_LJ_L (const char * arg1, I_64 arg2);
extern J9_CFUNC double  JNICALL C_x_co_LJ_D (const char * arg1, I_64 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_LL_L (const char * arg1, const char * arg2);
extern J9_CFUNC char*  JNICALL C_x_co_LD_L (const char * arg1, double arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_LL_I (const char * arg1, const char * arg2);
extern J9_CFUNC float  JNICALL C_x_co_LF_F (const char * arg1, float arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_LL_J (const char * arg1, const char * arg2);
extern J9_CFUNC double  JNICALL C_x_co_LI_D (const char * arg1, I_32 arg2);
extern J9_CFUNC float  JNICALL C_x_co_LL_F (const char * arg1, const char * arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_LD_I (const char * arg1, double arg2);
extern J9_CFUNC double  JNICALL C_x_co_LF_D (const char * arg1, float arg2);
extern J9_CFUNC double  JNICALL C_x_co_LD_D (const char * arg1, double arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_LJ_I (const char * arg1, I_64 arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_LF_I (const char * arg1, float arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_LJ_J (const char * arg1, I_64 arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_LD_J (const char * arg1, double arg2);
extern J9_CFUNC float  JNICALL C_x_co_LI_F (const char * arg1, I_32 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_LI_L (const char * arg1, I_32 arg2);
extern J9_CFUNC I_32  JNICALL C_x_co_LI_I (const char * arg1, I_32 arg2);
extern J9_CFUNC char*  JNICALL C_x_co_LF_L (const char * arg1, float arg2);
extern J9_CFUNC I_32  ASM_x_custom_co_IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII_I (I_32 arg1, I_32 arg2, I_32 arg3, I_32 arg4, I_32 arg5, I_32 arg6, I_32 arg7, I_32 arg8, I_32 arg9, I_32 arg10, I_32 arg11, I_32 arg12, I_32 arg13, I_32 arg14, I_32 arg15, I_32 arg16, I_32 arg17, I_32 arg18, I_32 arg19, I_32 arg20, I_32 arg21, I_32 arg22, I_32 arg23, I_32 arg24, I_32 arg25, I_32 arg26, I_32 arg27, I_32 arg28, I_32 arg29, I_32 arg30, I_32 arg31, I_32 arg32, I_32 arg33, I_32 arg34, I_32 arg35, I_32 arg36, I_32 arg37, I_32 arg38, I_32 arg39, I_32 arg40, I_32 arg41, I_32 arg42, I_32 arg43, I_32 arg44, I_32 arg45, I_32 arg46, I_32 arg47, I_32 arg48, I_32 arg49, I_32 arg50, I_32 arg51, I_32 arg52, I_32 arg53, I_32 arg54, I_32 arg55, I_32 arg56, I_32 arg57, I_32 arg58, I_32 arg59, I_32 arg60, I_32 arg61, I_32 arg62, I_32 arg63, I_32 arg64, I_32 arg65, I_32 arg66, I_32 arg67, I_32 arg68, I_32 arg69, I_32 arg70, I_32 arg71, I_32 arg72, I_32 arg73, I_32 arg74, I_32 arg75, I_32 arg76, I_32 arg77, I_32 arg78, I_32 arg79, I_32 arg80, I_32 arg81, I_32 arg82, I_32 arg83, I_32 arg84, I_32 arg85, I_32 arg86, I_32 arg87, I_32 arg88, I_32 arg89, I_32 arg90, I_32 arg91, I_32 arg92, I_32 arg93, I_32 arg94, I_32 arg95, I_32 arg96, I_32 arg97, I_32 arg98, I_32 arg99, I_32 arg100, I_32 arg101, I_32 arg102, I_32 arg103, I_32 arg104, I_32 arg105, I_32 arg106, I_32 arg107, I_32 arg108, I_32 arg109, I_32 arg110, I_32 arg111, I_32 arg112, I_32 arg113, I_32 arg114, I_32 arg115, I_32 arg116, I_32 arg117, I_32 arg118, I_32 arg119, I_32 arg120, I_32 arg121, I_32 arg122, I_32 arg123, I_32 arg124, I_32 arg125, I_32 arg126, I_32 arg127, I_32 arg128, I_32 arg129, I_32 arg130, I_32 arg131, I_32 arg132, I_32 arg133, I_32 arg134, I_32 arg135, I_32 arg136, I_32 arg137, I_32 arg138, I_32 arg139, I_32 arg140, I_32 arg141, I_32 arg142, I_32 arg143, I_32 arg144, I_32 arg145, I_32 arg146, I_32 arg147, I_32 arg148, I_32 arg149, I_32 arg150, I_32 arg151, I_32 arg152, I_32 arg153, I_32 arg154, I_32 arg155, I_32 arg156, I_32 arg157, I_32 arg158, I_32 arg159, I_32 arg160, I_32 arg161, I_32 arg162, I_32 arg163, I_32 arg164, I_32 arg165, I_32 arg166, I_32 arg167, I_32 arg168, I_32 arg169, I_32 arg170, I_32 arg171, I_32 arg172, I_32 arg173, I_32 arg174, I_32 arg175, I_32 arg176, I_32 arg177, I_32 arg178, I_32 arg179, I_32 arg180, I_32 arg181, I_32 arg182, I_32 arg183, I_32 arg184, I_32 arg185, I_32 arg186, I_32 arg187, I_32 arg188, I_32 arg189, I_32 arg190, I_32 arg191, I_32 arg192, I_32 arg193, I_32 arg194, I_32 arg195, I_32 arg196, I_32 arg197, I_32 arg198, I_32 arg199, I_32 arg200, I_32 arg201, I_32 arg202, I_32 arg203, I_32 arg204, I_32 arg205, I_32 arg206, I_32 arg207, I_32 arg208, I_32 arg209, I_32 arg210, I_32 arg211, I_32 arg212, I_32 arg213, I_32 arg214, I_32 arg215, I_32 arg216, I_32 arg217, I_32 arg218, I_32 arg219, I_32 arg220, I_32 arg221, I_32 arg222, I_32 arg223, I_32 arg224, I_32 arg225, I_32 arg226, I_32 arg227, I_32 arg228, I_32 arg229, I_32 arg230, I_32 arg231, I_32 arg232, I_32 arg233, I_32 arg234, I_32 arg235, I_32 arg236, I_32 arg237, I_32 arg238, I_32 arg239, I_32 arg240, I_32 arg241, I_32 arg242, I_32 arg243, I_32 arg244, I_32 arg245, I_32 arg246, I_32 arg247, I_32 arg248, I_32 arg249, I_32 arg250, I_32 arg251, I_32 arg252, I_32 arg253, I_32 arg254, I_32 arg255);
extern J9_CFUNC I_32  ASM_co_II_I (I_32 arg1, I_32 arg2);
extern J9_CFUNC float  JNICALL C_x_co_FD_F (float arg1, double arg2);
extern J9_CFUNC I_64  JNICALL C_x_co_FF_J (float arg1, float arg2);
extern J9_CFUNC double  JNICALL C_x_co_FL_D (float arg1, const char * arg2);
extern J9_CFUNC float  JNICALL C_x_co_FJ_F (float arg1, I_64 arg2);
#endif /* _J9INGCONVENTIONTESTS_ */

/* J9VMAsyncMessageHandler*/
#ifndef _J9VMASYNCMESSAGEHANDLER_
#define _J9VMASYNCMESSAGEHANDLER_
extern J9_CFUNC void  clearAsyncEventFlags (J9VMThread *vmThread, UDATA flags);
extern J9_CFUNC void  setAsyncEventFlags (J9VMThread *vmThread, UDATA flags, UDATA indicateEvent);
extern J9_CFUNC UDATA  javaCheckAsyncMessages (J9VMThread * vmThread, UDATA throwExceptions);
#endif /* _J9VMASYNCMESSAGEHANDLER_ */

/* J9VMBigIntegerSupport*/
#ifndef _J9VMBIGINTEGERSUPPORT_
#define _J9VMBIGINTEGERSUPPORT_
extern J9_CFUNC jlongArray  internalBigIntegerAdd (JNIEnv * env, jlongArray src1, jlongArray src2);
extern J9_CFUNC jlongArray  bigIntegerNeg (JNIEnv * env, jlongArray src);
extern J9_CFUNC jlongArray  internalBigIntegerNeg (JNIEnv * env, jlongArray src1);
extern J9_CFUNC jlongArray  internalBigIntegerNormalize (JNIEnv * env, jlongArray src1);
extern J9_CFUNC jlongArray  bigIntegerAdd (JNIEnv * env, jlongArray src1, jlongArray src2);
#endif /* _J9VMBIGINTEGERSUPPORT_ */

/* J9VMBigJNICall*/
#ifndef _J9VMBIGJNICALL_
#define _J9VMBIGJNICALL_
#if !defined(J9VM_INTERP_MINIMAL_JNI)
extern J9_CFUNC UDATA  dispatchBigJNICall (J9VMThread *vmThread, void *functionAddress, UDATA returnType, void *javaArgs, UDATA javaArgsCount, void *receiverAddress, void *types, UDATA bp);
#endif /* J9VM_INTERP_MINIMAL_JNI */
#endif /* _J9VMBIGJNICALL_ */

/* J9VMClassSupport*/
#ifndef _J9VMCLASSSUPPORT_
#define _J9VMCLASSSUPPORT_
extern J9_CFUNC UDATA  internalCreateBaseTypePrimitiveAndArrayClasses (J9VMThread *currentThread);
extern J9_CFUNC struct J9Class*  internalFindClassUTF8 (J9VMThread *currentThread, U_8 *className, UDATA classNameLength, J9ClassLoader *classLoader, UDATA options);
extern J9_CFUNC struct J9Class*  internalFindKnownClass (J9VMThread *currentThread, UDATA index, UDATA flags);
#endif /* _J9VMCLASSSUPPORT_ */

/* J9SourceJvmriSupport*/
extern J9_CFUNC void
rasStartDeferredThreads PROTOTYPE((J9JavaVM* vm));

extern J9_CFUNC int
initJVMRI PROTOTYPE(( J9JavaVM * vm ));

struct DgRasInterface ;
extern J9_CFUNC int 
fillInDgRasInterface PROTOTYPE((struct DgRasInterface *dri));

extern J9_CFUNC int
shutdownJVMRI PROTOTYPE(( J9JavaVM * vm ));

/* J9VMCmdSetThread*/
#ifndef _J9VMCMDSETTHREAD_
#define _J9VMCMDSETTHREAD_
extern J9_CFUNC void  jdwp_thread_getThreadgroup (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_thread_resume (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_thread_getStatus (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_thread_interrupt (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_thread_suspendCount (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_thread_getFrameCount (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_thread_getName (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_thread_suspend (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_thread_getCurrentContendedMonitor (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_thread_stop (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_thread_getOwnedMonitors (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_thread_getFrames (J9VMThread *currentThread);
#endif /* _J9VMCMDSETTHREAD_ */

/* J9VMCmdSetThreadgroup*/
#ifndef _J9VMCMDSETTHREADGROUP_
#define _J9VMCMDSETTHREADGROUP_
extern J9_CFUNC void  jdwp_threadgroup_getChildren (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_threadgroup_getName (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_threadgroup_getParent (J9VMThread *currentThread);
#endif /* _J9VMCMDSETTHREADGROUP_ */

/* J9VMCmdSetVM*/
#ifndef _J9VMCMDSETVM_
#define _J9VMCMDSETVM_
extern J9_CFUNC void  jdwp_vm_allClasses (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_unimplemented (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_capabilities_new (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_allThreads (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_exit (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_resume (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_version (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_classesForSignature (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_classPaths (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_holdEvents (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_releaseEvents (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_createString (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_topLevelThreadgroup (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_idSizes (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_setDefaultStratum (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_dispose (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_redefineClasses (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_suspend (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_capabilities (J9VMThread *currentThread);
extern J9_CFUNC void  jdwp_vm_disposeObjects (J9VMThread *currentThread);
#endif /* _J9VMCMDSETVM_ */


/* J9VMFloatSupport*/
#ifndef _J9VMFLOATSUPPORT_
#define _J9VMFLOATSUPPORT_
extern J9_CFUNC void  helperInitializeFPU (void);
#endif /* _J9VMFLOATSUPPORT_ */

/* J9VMInitializeVM*/
#ifndef _J9VMINITIALIZEVM_
#define _J9VMINITIALIZEVM_
extern J9_CFUNC void  initializeExecutionModel (J9VMThread *currentThread);
#endif /* _J9VMINITIALIZEVM_ */

/* J9VMMethodUtils*/
#ifndef _J9VMMETHODUTILS_
#define _J9VMMETHODUTILS_
extern J9_CFUNC struct J9ROMMethod*  nextROMMethod (J9ROMMethod * romMethod);
extern J9_CFUNC J9MethodParametersData * methodParametersFromROMMethod(J9ROMMethod *romMethod);
extern J9_CFUNC J9MethodParametersData * getMethodParametersFromROMMethod(J9ROMMethod *romMethod);
extern J9_CFUNC U_32 getExtendedModifiersDataFromROMMethod(J9ROMMethod *romMethod);
extern J9_CFUNC U_32 * getMethodAnnotationsDataFromROMMethod(J9ROMMethod *romMethod);
extern J9_CFUNC U_32 * getParameterAnnotationsDataFromROMMethod(J9ROMMethod *romMethod);
extern J9_CFUNC U_32 * getMethodTypeAnnotationsDataFromROMMethod(J9ROMMethod *romMethod);
extern J9_CFUNC U_32 * getCodeTypeAnnotationsDataFromROMMethod(J9ROMMethod *romMethod);
extern J9_CFUNC U_32 * getDefaultAnnotationDataFromROMMethod(J9ROMMethod *romMethod);
extern J9_CFUNC J9MethodDebugInfo * methodDebugInfoFromROMMethod(J9ROMMethod *romMethod);
extern J9_CFUNC J9MethodDebugInfo * getMethodDebugInfoFromROMMethod(J9ROMMethod *romMethod);
extern J9_CFUNC U_32*  stackMapFromROMMethod(J9ROMMethod * romMethod);
extern J9_CFUNC UDATA * getNextStackMapFrame(U_32 *stackMap, UDATA *previousFrame);
#endif /* _J9VMMETHODUTILS_ */

/* J9VMJITDebugHelpers*/
#ifndef _J9VMJITDEBUGHELPERS_
#define _J9VMJITDEBUGHELPERS_
#if defined(J9VM_JIT_FULL_SPEED_DEBUG) && defined(J9VM_INTERP_NATIVE_SUPPORT)
extern J9_CFUNC void  jitResetAllUntranslateableMethods (J9VMThread *vmThread);
#endif /* J9VM_JIT_FULL_SPEED_DEBUG &&  J9VM_INTERP_NATIVE_SUPPORT */

extern J9_CFUNC void  jitConvertStoredDoubleRegisterToSingle (U_64 * doublePtr, U_32 * singlePtr);
#endif /* _J9VMJITDEBUGHELPERS_ */

/* J9VMJITNativeCompileSupport*/
#ifndef _J9VMJITNATIVECOMPILESUPPORT_
#define _J9VMJITNATIVECOMPILESUPPORT_
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
extern J9_CFUNC UDATA  jitGetFieldType (UDATA cpIndex, J9Method *ramMethod);
extern J9_CFUNC UDATA  jitCTInstanceOf (J9Class *instanceClass, J9Class *castClass);
extern J9_CFUNC struct J9ROMFieldShape*  jitGetInstanceFieldShape (J9VMThread *vmContext, J9Class *classOfInstance, J9ConstantPool *constantPool, UDATA fieldIndex);
extern J9_CFUNC struct J9Class*  jitGetClassOfFieldFromCP (J9VMThread *vmContext, J9ConstantPool *constantPool, UDATA cpIndex);
extern J9_CFUNC struct J9Class*  jitGetClassFromUTF8 (J9VMThread *vmStruct, J9ConstantPool *callerCP, void *signatureChars, UDATA signatureLength);
extern J9_CFUNC struct J9Class*  jitGetClassInClassloaderFromUTF8 (J9VMThread *vmStruct, J9ClassLoader *classLoader, void *signatureChars, UDATA signatureLength);
extern J9_CFUNC UDATA  jitFieldsAreIdentical (J9VMThread *vmStruct, J9ConstantPool *cp1, UDATA index1, J9ConstantPool *cp2, UDATA index2, UDATA isStatic);
extern J9_CFUNC IDATA  jitCTResolveInstanceFieldRefWithMethod (J9VMThread *vmStruct, J9Method *method, UDATA fieldIndex, UDATA resolveFlags, J9ROMFieldShape **resolvedField);
extern J9_CFUNC struct J9Method*  jitGetInterfaceMethodFromCP (J9VMThread *vmThread, J9ConstantPool *constantPool, UDATA cpIndex, J9Class* lookupClass);
extern J9_CFUNC struct J9UTF8* jitGetConstantDynamicTypeFromCP(J9VMThread *currentThread, J9ConstantPool *constantPool, UDATA cpIndex);
extern J9_CFUNC void*  jitGetCountingSendTarget (J9VMThread *vmThread, J9Method *ramMethod);
extern J9_CFUNC void  jitResetAllMethodsAtStartup (J9VMThread *vmContext);
extern J9_CFUNC struct J9Class*  jitGetInterfaceITableIndexFromCP (J9VMThread *vmThread, J9ConstantPool *constantPool, UDATA cpIndex, UDATA* pITableIndex);
extern J9_CFUNC struct J9Method*  jitGetImproperInterfaceMethodFromCP (J9VMThread *vmThread, J9ConstantPool *constantPool, UDATA cpIndex, UDATA* nonFinalObjectMethodVTableOffset);
extern J9_CFUNC void  jitAcquireClassTableMutex (J9VMThread *vmThread);
extern J9_CFUNC void*  jitCTResolveStaticFieldRefWithMethod (J9VMThread *vmStruct, J9Method *method, UDATA fieldIndex, UDATA resolveFlags, J9ROMFieldShape **resolvedField);
extern J9_CFUNC void  jitReleaseClassTableMutex (J9VMThread *vmThread);
extern J9_CFUNC UDATA  jitGetInterfaceVTableOffsetFromCP (J9VMThread *vmThread, J9ConstantPool *constantPool, UDATA cpIndex, J9Class* lookupClass);
extern J9_CFUNC void  jitParseSignature (const J9UTF8 *signature, U_8 *paramBuffer, UDATA *paramElements, UDATA *parmSlots);
extern J9_CFUNC UDATA jitMethodEnterTracingEnabled(J9VMThread *currentThread, J9Method *method);
extern J9_CFUNC UDATA jitMethodExitTracingEnabled(J9VMThread *currentThread, J9Method *method);
extern J9_CFUNC UDATA jitMethodIsBreakpointed(J9VMThread *currentThread, J9Method *method);

extern J9_CFUNC IDATA jitPackedArrayFieldLength(J9VMThread *vmStruct, J9ConstantPool *constantPool, UDATA arrayFieldIndex);
extern J9_CFUNC IDATA jitIsPackedFieldNested(J9VMThread *vmStruct, J9ConstantPool *constantPool, UDATA fieldIndex);

extern J9_CFUNC UDATA jitGetRealCPIndex(J9VMThread *currentThread, J9ROMClass *romClass, UDATA cpOrSplitIndex);
extern J9_CFUNC struct J9Method* jitGetJ9MethodUsingIndex(J9VMThread *currentThread, J9ConstantPool *constantPool, UDATA cpOrSplitIndex);
extern J9_CFUNC struct J9Method* jitResolveStaticMethodRef(J9VMThread *vmStruct, J9ConstantPool *constantPool, UDATA cpOrSplitIndex, UDATA resolveFlags);
extern J9_CFUNC struct J9Method* jitResolveSpecialMethodRef(J9VMThread *vmStruct, J9ConstantPool *constantPool, UDATA cpOrSplitIndex, UDATA resolveFlags);
extern J9_CFUNC struct J9Class * jitGetDeclaringClassOfROMField(J9VMThread *vmStruct, J9Class *clazz, J9ROMFieldShape *romField);

typedef struct J9MethodFromSignatureWalkState {
	const char *className;
	U_32 classNameLength;
	struct J9JNINameAndSignature nameAndSig;
	struct J9VMThread *vmThread;
	struct J9ClassLoaderWalkState classLoaderWalkState;
} J9MethodFromSignatureWalkState;

/**
 * Cleans up the iterator after matching all methods found.
 *
 * @param state The inlialized iterator state
 */
extern J9_CFUNC void
allMethodsFromSignatureEndDo(J9MethodFromSignatureWalkState *state);

/**
 * Advances the iterator looking for the next method matching the exact signature provided during initialization.
 *
 * @param state The inlialized iterator state
 *
 * @return      J9Method matching the exact signature provided during initialization of the iterator in the class loader
 *              if it exists; NULL if no such method is found.
 */
extern J9_CFUNC struct J9Method *
allMethodsFromSignatureNextDo(J9MethodFromSignatureWalkState *state);

/**
 * Initializes the iterator state and returns the first method matching the supplied exact (no wildcards allowed)
 * signature from any class loader.
 *
 * @param state            The iterator state which will be initialized.
 * @param vm               The VM instance to look for methods in.
 * @param flags            The flags forwarded to class loader iterator functions
 * @param className        The class name including the package.
 * @param classNameLength  The length (in number of bytes) of the class name
 * @param methodName       The method name
 * @param methodNameLength The length (in number of bytes) of the method name
 * @param methodSig        The method signature
 * @param methodSigLength  The length (in number of bytes) of the method signature
 *
 * @return                 J9Method matching the exact signature provided in the first class loader it is found; NULL if
 *                         no such method is found.
 */
extern J9_CFUNC struct J9Method *
allMethodsFromSignatureStartDo(J9MethodFromSignatureWalkState *state, J9JavaVM* vm, UDATA flags, const char *className, U_32 classNameLength, const char *methodName, U_32 methodNameLength, const char *methodSig, U_32 methodSigLength);

#endif /* J9VM_INTERP_NATIVE_SUPPORT*/
#endif /* _J9VMJITNATIVECOMPILESUPPORT_ */

/* J9VMJITRuntimeHelpers*/
#ifndef _J9VMJITRUNTIMEHELPERS_
#define _J9VMJITRUNTIMEHELPERS_
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
extern J9_CFUNC void  jitMarkMethodReadyForDLT (J9VMThread *currentThread, J9Method * method);
extern J9_CFUNC void  jitMethodTranslated (J9VMThread *currentThread, J9Method *method, void *jitStartAddress);
extern J9_CFUNC void  jitNewInstanceMethodTranslated (J9VMThread *currentThread, J9Class *clazz, void *jitStartAddress);
extern J9_CFUNC void  jitMethodFailedTranslation (J9VMThread *currentThread, J9Method *method);
extern J9_CFUNC void  jitCheckScavengeOnResolve (J9VMThread *currentThread);
extern J9_CFUNC void*  jitNewInstanceMethodStartAddress (J9VMThread *currentThread, J9Class *clazz);
extern J9_CFUNC void  initializeCodertFunctionTable (J9JavaVM *javaVM);
extern J9_CFUNC void  jitNewInstanceMethodTranslateFailed (J9VMThread *currentThread, J9Class *clazz);
extern J9_CFUNC UDATA  jitUpdateCount (J9VMThread *currentThread, J9Method *method, UDATA oldCount, UDATA newCount);
extern J9_CFUNC void  jitUpdateInlineAttribute (J9VMThread *currentThread, J9Class * classPtr, void *jitCallBack);
extern J9_CFUNC UDATA  jitTranslateMethod (J9VMThread *vmThread, J9Method *method);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */
#endif /* _J9VMJITRUNTIMEHELPERS_ */

/* J9VMNativeInterfaceSupport*/
#ifndef _J9VMNATIVEINTERFACESUPPORT_
#define _J9VMNATIVEINTERFACESUPPORT_
extern J9_CFUNC U_8*  compressUTF8 (J9VMThread * vmThread, U_8 * data, UDATA length, UDATA * compressedLength);
extern J9_CFUNC jlong  JNICALL getStaticLongField (JNIEnv *env, jclass clazz, jfieldID fieldID);
extern J9_CFUNC void  JNICALL setStaticIntField (JNIEnv *env, jclass clazz, jfieldID fieldID, jint value);
extern J9_CFUNC void  JNICALL setStaticFloatField (JNIEnv *env, jclass cls, jfieldID fieldID, jfloat value);
extern J9_CFUNC void  JNICALL setStaticDoubleField (JNIEnv *env, jclass cls, jfieldID fieldID, jdouble value);
extern J9_CFUNC void  JNICALL setStaticLongField (JNIEnv *env, jclass cls, jfieldID fieldID, jlong value);
extern J9_CFUNC void  JNICALL getStringUTFRegion (JNIEnv *env, jstring str, jsize start, jsize len, char *buf);
extern J9_CFUNC jint  JNICALL getStaticIntField (JNIEnv *env, jclass clazz, jfieldID fieldID);
extern J9_CFUNC void  JNICALL releaseStringChars (JNIEnv *env, jstring string, const jchar * chars);
extern J9_CFUNC void  JNICALL releaseStringCharsUTF (JNIEnv *env, jstring string, const char * chars);
extern J9_CFUNC jint  JNICALL unregisterNatives (JNIEnv *env, jclass clazz);
extern J9_CFUNC struct J9Method*  findJNIMethod (J9VMThread* vmStruct, J9Class* clazz, char* name, char* signature);
extern J9_CFUNC jfloat  JNICALL getStaticFloatField (JNIEnv *env, jclass clazz, jfieldID fieldID);
extern J9_CFUNC void  JNICALL deleteLocalRef (JNIEnv *env, jobject localRef);
extern J9_CFUNC void  jniResetStackReferences (JNIEnv *env);
extern J9_CFUNC jboolean  JNICALL isAssignableFrom (JNIEnv *env, jclass clazz1, jclass clazz2);
extern J9_CFUNC jclass  JNICALL getObjectClass (JNIEnv *env, jobject obj);
extern J9_CFUNC jmethodID  JNICALL fromReflectedMethod (JNIEnv *env, jobject method);
extern J9_CFUNC jfieldID  JNICALL fromReflectedField (JNIEnv *env, jobject field);
extern J9_CFUNC jstring  JNICALL newStringUTF (JNIEnv *env, const char *bytes);
extern J9_CFUNC const jchar*  JNICALL getStringChars (JNIEnv *env, jstring string, jboolean *isCopy);
extern J9_CFUNC jobjectArray  JNICALL newObjectArray (JNIEnv *env, jsize length, jclass elementClass, jobject initialElement);
extern J9_CFUNC jsize  JNICALL getArrayLength (JNIEnv *env, jarray array);
extern J9_CFUNC jint  JNICALL jniThrow (JNIEnv *env, jthrowable obj);
extern J9_CFUNC void  JNICALL releaseArrayElements (JNIEnv *env, jarray array, void * elems, jint mode);
extern J9_CFUNC void  JNICALL setStaticObjectField (JNIEnv *env, jclass clazz, jfieldID fieldID, jobject value);
extern J9_CFUNC jobject  JNICALL getStaticObjectField (JNIEnv *env, jclass clazz, jfieldID fieldID);
extern J9_CFUNC jsize  JNICALL getStringUTFLength (JNIEnv *env, jstring string);
extern J9_CFUNC jint  JNICALL registerNatives (JNIEnv *env, jclass clazz, const JNINativeMethod *methods, jint nMethods);
extern J9_CFUNC jdouble  JNICALL getStaticDoubleField (JNIEnv *env, jclass clazz, jfieldID fieldID);
extern J9_CFUNC jobject  JNICALL allocObject (JNIEnv *env, jclass clazz);
extern J9_CFUNC jclass  JNICALL getSuperclass (JNIEnv *env, jclass clazz);
extern J9_CFUNC jsize  JNICALL getStringLength (JNIEnv *env, jstring string);
extern J9_CFUNC void  JNICALL setArrayRegion (JNIEnv *env, jarray array, jsize start, jsize len, void * buf);
extern J9_CFUNC void  JNICALL getArrayRegion (JNIEnv *env, jarray array, jsize start, jsize len, void * buf);
extern J9_CFUNC jclass  JNICALL findClass (JNIEnv *env, const char *name);
extern J9_CFUNC void  returnFromJNI (J9VMThread *env, void * bp);
extern J9_CFUNC const char*  JNICALL getStringUTFChars (JNIEnv *env, jstring string, jboolean *isCopy);
extern J9_CFUNC jint  JNICALL getVersion (JNIEnv *env);
extern J9_CFUNC void  JNICALL getStringRegion (JNIEnv *env, jstring str, jsize start, jsize len, jchar *buf);
extern J9_CFUNC jobject  newBaseTypeArray (JNIEnv *env, IDATA length, UDATA arrayClassOffset);
extern J9_CFUNC jboolean  JNICALL isSameObject (JNIEnv *env, jobject ref1, jobject ref2);
extern J9_CFUNC void*  JNICALL getArrayElements (JNIEnv *env, jarray array, jboolean *isCopy);
#endif /* _J9VMNATIVEINTERFACESUPPORT_ */

/* J9VMResolveSupport*/
#ifndef _J9VMRESOLVESUPPORT_
#define _J9VMRESOLVESUPPORT_
extern J9_CFUNC void  atomicOrIntoConstantPool (J9JavaVM *vm, J9Method *method, UDATA bits);
extern J9_CFUNC void  atomicAndIntoConstantPool (J9JavaVM *vm, J9Method *method, UDATA bits);
extern J9_CFUNC j9object_t resolveMethodTypeRef (J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags);
extern J9_CFUNC j9object_t resolveMethodTypeRefInto(J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9RAMMethodTypeRef *ramCPEntry);
extern J9_CFUNC j9object_t resolveMethodHandleRef (J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags);
extern J9_CFUNC j9object_t resolveMethodHandleRefInto(J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9RAMMethodHandleRef *ramCPEntry);
extern J9_CFUNC j9object_t resolveOpenJDKInvokeHandle (J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags);
extern J9_CFUNC j9object_t resolveInvokeDynamic (J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags);
extern J9_CFUNC j9object_t resolveConstantDynamic (J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags);
#endif /* _J9VMRESOLVESUPPORT_ */

/* J9VMRomImageSupport*/
#ifndef _J9VMROMIMAGESUPPORT_
#define _J9VMROMIMAGESUPPORT_
#if defined(J9VM_IVE_ROM_IMAGE_HELPERS)
extern J9_CFUNC UDATA  romImageToMemorySegment (JNIEnv *jniEnv, void *romImagePtr);
extern J9_CFUNC UDATA  romImageLoad (J9VMThread *currentThread, void *segmentPointer, jobject classLoader, void *jxePointer, void *jxeAlloc);
#endif /* J9VM_IVE_ROM_IMAGE_HELPERS */
#endif /* _J9VMROMIMAGESUPPORT_ */


/* J9VMStringSupport*/
#ifndef _J9VMSTRINGSUPPORT_
#define _J9VMSTRINGSUPPORT_
extern J9_CFUNC j9object_t  catUtfToString4 (J9VMThread * vmThread, const U_8 *data1, UDATA length1, const U_8 *data2, UDATA length2, const U_8 *data3, UDATA length3, const U_8 *data4, UDATA length4);
extern J9_CFUNC j9object_t  methodToString (J9VMThread * vmThread, J9Method* method);
#endif /* _J9VMSTRINGSUPPORT_ */

/* J9VMTasukiMonitor*/
#ifndef _J9VMTASUKIMONITOR_
#define _J9VMTASUKIMONITOR_
extern J9_CFUNC UDATA  objectMonitorEnterBlocking (J9VMThread *currentThread);
extern J9_CFUNC UDATA  objectMonitorEnterNonBlocking (J9VMThread *currentThread, j9object_t object);
extern J9_CFUNC void  monitorExitWriteBarrier ();
extern J9_CFUNC void  incrementCancelCounter (J9Class *clazz);
#endif /* _J9VMTASUKIMONITOR_ */

/* J9VMUTF8Support*/
#ifndef _J9VMUTF8SUPPORT_
#define _J9VMUTF8SUPPORT_
extern J9_CFUNC UDATA  computeHashForUTF8 (const U_8 * string, UDATA length);
#endif /* _J9VMUTF8SUPPORT_ */

/* J9VMDebuggerExtensionUTF8Support*/
#ifndef _J9VMDEBUGGEREXTENSIONUTF8SUPPORT_
#define _J9VMDEBUGGEREXTENSIONUTF8SUPPORT_
extern J9_CFUNC void  dbgDumpStack (J9VMThread *vmThread, IDATA delta);
#endif /* _J9VMDEBUGGEREXTENSIONUTF8SUPPORT_ */

/* J9VMVisibility*/
#ifndef _J9VMVISIBILITY_
#define _J9VMVISIBILITY_
extern J9_CFUNC IDATA  checkVisibility (J9VMThread* currentThread, J9Class* sourceClass, J9Class* destClass, UDATA modifiers, UDATA lookupOptions);
#endif /* _J9VMVISIBILITY_ */

/* J9VMVolatileLongFunctions*/
#ifndef _J9VMVOLATILELONGFUNCTIONS_
#define _J9VMVOLATILELONGFUNCTIONS_
#if !defined(J9VM_ENV_DATA64)
extern J9_CFUNC U_64  longVolatileRead (J9VMThread *vmThread, U_64 * srcAddress);
extern J9_CFUNC void  longVolatileWrite (J9VMThread *vmThread, U_64 * destAddress, U_64 * value);
#endif /* J9VM_ENV_DATA64 */
#endif /* _J9VMVOLATILELONGFUNCTIONS_ */

/* J9VMDropHelpers*/
#ifndef _J9VMDROPHELPERS_
#define _J9VMDROPHELPERS_
extern J9_CFUNC void  prepareForExceptionThrow (J9VMThread *currentThread);
extern J9_CFUNC UDATA  dropPendingSendPushes (J9VMThread *currentThread);
#endif /* _J9VMDROPHELPERS_ */

/* J9VMJavaInterpreterStartup*/
#ifndef _J9VMJAVAINTERPRETERSTARTUP_
#define _J9VMJAVAINTERPRETERSTARTUP_
extern J9_CFUNC void  JNICALL cleanUpAttachedThread (J9VMThread *vmContext);
extern J9_CFUNC void  JNICALL handleUncaughtException (J9VMThread *vmContext);
extern J9_CFUNC void  JNICALL sidecarInvokeReflectMethod (J9VMThread *vmContext, jobject methodRef, jobject recevierRef, jobjectArray argsRef);
extern J9_CFUNC void  JNICALL sidecarInvokeReflectMethodImpl (J9VMThread *vmContext, jobject methodRef, jobject recevierRef, jobjectArray argsRef);
extern J9_CFUNC void  JNICALL sendInit (J9VMThread *vmContext, j9object_t object, J9Class *senderClass, UDATA lookupOptions);
extern J9_CFUNC void  JNICALL printStackTrace (J9VMThread *vmContext, j9object_t exception);
extern J9_CFUNC void  JNICALL sendLoadClass (J9VMThread *vmContext, j9object_t classLoaderObject, j9object_t classNameObject);
extern J9_CFUNC void  JNICALL runJavaThread (J9VMThread *vmContext);
extern J9_CFUNC void  JNICALL sendCompleteInitialization (J9VMThread *vmContext);
extern J9_CFUNC void  JNICALL sendClinit (J9VMThread *vmContext, J9Class *clazz);
extern J9_CFUNC void  JNICALL sendInitializationAlreadyFailed (J9VMThread *vmContext, J9Class *clazz);
extern J9_CFUNC void  JNICALL sendRecordInitializationFailure (J9VMThread *vmContext, J9Class *clazz, j9object_t throwable);
extern J9_CFUNC void  JNICALL runCallInMethod (JNIEnv *env, jobject receiver, jclass clazz, jmethodID methodID, void* args);
extern J9_CFUNC void  JNICALL internalSendExceptionConstructor (J9VMThread *vmContext, J9Class *exceptionClass, j9object_t exception, j9object_t detailMessage, UDATA constructorIndex);
extern J9_CFUNC void  JNICALL sendInitCause (J9VMThread *vmContext, j9object_t receiver, j9object_t cause);
extern J9_CFUNC void  JNICALL initializeAttachedThread (J9VMThread *vmContext, const char *name, j9object_t *group, UDATA daemon, J9VMThread *initializee);
extern J9_CFUNC void  JNICALL initializeAttachedThreadImpl (J9VMThread *vmContext, const char *name, j9object_t *group, UDATA daemon, J9VMThread *initializee);
extern J9_CFUNC void  JNICALL runStaticMethod (J9VMThread *vmContext, U_8* className, J9NameAndSignature* selector, UDATA argCount, UDATA* arguments);
extern J9_CFUNC void  JNICALL internalRunStaticMethod (J9VMThread *vmContext, J9Method *method, BOOLEAN returnsObject, UDATA argCount, UDATA* arguments);
extern J9_CFUNC void  JNICALL sendCheckPackageAccess (J9VMThread *vmContext, J9Class * clazz, j9object_t protectionDomain);
extern J9_CFUNC void  JNICALL sidecarInvokeReflectConstructor (J9VMThread *vmContext, jobject constructorRef, jobject recevierRef, jobjectArray argsRef);
extern J9_CFUNC void  JNICALL sidecarInvokeReflectConstructorImpl (J9VMThread *vmContext, jobject constructorRef, jobject recevierRef, jobjectArray argsRef);
extern J9_CFUNC void  JNICALL sendFromMethodDescriptorString (J9VMThread *vmThread, J9UTF8 *descriptor, J9ClassLoader *classLoader, J9Class *appendArgType);
extern J9_CFUNC void  JNICALL sendResolveMethodHandle (J9VMThread *vmThread, UDATA cpIndex, J9ConstantPool *ramCP, J9Class *definingClass, J9ROMNameAndSignature* nameAndSig);
extern J9_CFUNC void  JNICALL sendForGenericInvoke (J9VMThread *vmThread, j9object_t methodHandle, j9object_t methodType, UDATA dropFirstArg);
extern J9_CFUNC void  JNICALL sendResolveOpenJDKInvokeHandle (J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, I_32 refKind, J9Class *resolvedClass, J9ROMNameAndSignature* nameAndSig);
extern J9_CFUNC void  JNICALL sendResolveConstantDynamic (J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA cpIndex, J9ROMNameAndSignature* nameAndSig, U_16* bsmData);
extern J9_CFUNC void  JNICALL sendResolveInvokeDynamic (J9VMThread *vmThread, J9ConstantPool *ramCP, UDATA callSiteIndex, J9ROMNameAndSignature* nameAndSig, U_16* bsmData);
extern J9_CFUNC void  JNICALL jitFillOSRBuffer (struct J9VMThread *vmContext, void *osrBlock);
extern J9_CFUNC void  JNICALL sendRunThread(J9VMThread *vmContext, j9object_t tenantContext);
#endif /* _J9VMJAVAINTERPRETERSTARTUP_ */

/* J9VMNativeHelpersLarge*/
#ifndef _J9VMNATIVEHELPERSLARGE_
#define _J9VMNATIVEHELPERSLARGE_
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
extern J9_CFUNC U_32  getJitRegisterMap (J9JITExceptionTable *metadata, void * stackMap);
extern J9_CFUNC void*  getJitInlinedCallInfo (J9JITExceptionTable * md);
extern J9_CFUNC void*  getFirstInlinedCallSite (J9JITExceptionTable * metaData, void * stackMap);
extern J9_CFUNC UDATA  getByteCodeIndexFromStackMap (J9JITExceptionTable *metaData, void *stackMap);
extern J9_CFUNC void*  getInlinedMethod (void * inlinedCallSite);
extern J9_CFUNC UDATA  hasMoreInlinedMethods (void * inlinedCallSite);
extern J9_CFUNC UDATA  getByteCodeIndex (void * inlinedCallSite);
extern J9_CFUNC void  jitReportDynamicCodeLoadEvents (J9VMThread * currentThread);
extern J9_CFUNC UDATA  getJitInlineDepthFromCallSite (J9JITExceptionTable *metaData, void *inlinedCallSite);
extern J9_CFUNC void*  jitGetInlinerMapFromPC (J9JavaVM * javaVM, J9JITExceptionTable * exceptionTable, UDATA jitPC);
extern J9_CFUNC void*  getStackMapFromJitPC (J9JavaVM * javaVM, struct J9JITExceptionTable * exceptionTable, UDATA jitPC);
extern J9_CFUNC UDATA  getCurrentByteCodeIndexAndIsSameReceiver (J9JITExceptionTable *metaData, void *stackMap, void *currentInlinedCallSite, UDATA * isSameReceiver);
extern J9_CFUNC void  clearFPStack (void);
extern J9_CFUNC void  jitExclusiveVMShutdownPending (J9VMThread *vmThread);
extern J9_CFUNC void*  getNextInlinedCallSite (J9JITExceptionTable * metaData, void * inlinedCallSite);
extern J9_CFUNC void*  jitGetStackMapFromPC (J9JavaVM * javaVM, struct J9JITExceptionTable * exceptionTable, UDATA jitPC);
extern J9_CFUNC void  jitAddPicToPatchOnClassUnload (void *classPointer, void *addressToBePatched);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */
#endif /* _J9VMNATIVEHELPERSLARGE_ */

#ifdef __cplusplus
}
#endif

#endif /* J9PROTOS_H */

