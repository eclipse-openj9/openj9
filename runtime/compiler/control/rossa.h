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

#ifndef __ROSSA_H
#define __ROSSA_H

typedef enum {
   compilationOK                                   = 0,
   compilationFailure                              = 1,  /* catch all error */
   compilationRestrictionILNodes                   = 2,  /* Unused */
   compilationRestrictionRecDepth                  = 3,  /* Unused */
   compilationRestrictedMethod                     = 4,  /* filters, JNI, abstract */
   compilationExcessiveComplexity                  = 5,
   compilationNotNeeded                            = 6,
   compilationSuspended                            = 7,
   compilationExcessiveSize                        = 8,  /* full caches */
   compilationInterrupted                          = 9,  /* GC wants to unload classes */
   compilationMetaDataFailure                      = 10, /* cannot create metadata */
   compilationInProgress                           = 11, /* for async compilations */
   compilationCHTableCommitFailure                 = 12,
   compilationMaxCallerIndexExceeded               = 13,
   compilationKilledByClassReplacement             = 14,
   compilationHeapLimitExceeded                    = 15,
   compilationNeededAtHigherLevel                  = 16, /*rtj */
   compilationAotValidateFieldFailure              = 17, /* aot failures */
   compilationAotStaticFieldReloFailure            = 18,
   compilationAotClassReloFailure                  = 19,
   compilationAotThunkReloFailure                  = 20,
   compilationAotTrampolineReloFailure             = 21,
   compilationAotPicTrampolineReloFailure          = 22,
   compilationAotCacheFullReloFailure              = 23, /* end of aot failures */
   compilationAotUnknownReloTypeFailure            = 24,
   compilationCodeReservationFailure               = 25,
   compilationAotHasInvokehandle                   = 26,
   compilationTrampolineFailure                    = 27,
   compilationRecoverableTrampolineFailure         = 28, /* we should retry these */
   compilationILGenFailure                         = 29,
   compilationIllegalCodeCacheSwitch               = 30,
   compilationNullSubstituteCodeCache              = 31,
   compilationCodeMemoryExhausted                  = 32,
   compilationGCRPatchFailure                      = 33,
   compilationAotValidateMethodExitFailure         = 34,
   compilationAotValidateMethodEnterFailure        = 35,
   compilationLambdaEnforceScorching               = 37,
   compilationInternalPointerExceedLimit           = 38,
   compilationAotRelocationInterrupted             = 39,
   compilationAotClassChainPersistenceFailure      = 40,
   compilationLowPhysicalMemory                    = 41,
   compilationDataCacheError                       = 42,
   compilationCodeCacheError                       = 43,
   compilationRecoverableCodeCacheError            = 44,
   compilationAotHasInvokeVarHandle                = 45,
   compilationAotValidateStringCompressionFailure  = 46,
   compilationFSDHasInvokeHandle                   = 47,
   compilationVirtualAddressExhaustion             = 48,
   compilationEnforceProfiling                     = 49,
   compilationSymbolValidationManagerFailure       = 50,
   compilationAOTNoSupportForAOTFailure            = 51,
   compilationAOTValidateTMFailure                 = 52,
   /* please insert new codes before compilationMaxError which is used in jar2jxe to test the error codes range */
   /* If new codes are added then add the corresponding names in compilationErrorNames table in rossa.cpp */
   compilationMaxError /* must be the last one */
} TR_CompilationErrorCode;

#ifdef __cplusplus
extern "C" {
#endif
        /* compilation error codes */

   jint JNICALL JVM_OnUnload(JavaVM * jvm, void* reserved0);
   IDATA j9jit_testarossa(struct J9JITConfig *jitConfig, J9VMThread * context, J9Method * method, void *oldStartPC);
   IDATA j9jit_testarossa_err(struct J9JITConfig *jitConfig, J9VMThread * context, J9Method * method, void * oldStartPC, TR_CompilationErrorCode *compErrCode);
   IDATA retranslateWithPreparation(struct J9JITConfig * jitConfig, J9VMThread * vmThread, J9Method * method, void * oldStartPC, UDATA reason);
   IDATA retranslateWithPreparationForMethodRedefinition(struct J9JITConfig * jitConfig, J9VMThread * vmThread, J9Method * method, void * oldStartPC);
   void* old_translateMethodHandle(J9VMThread *currentThread, j9object_t methodHandle);
   void* translateMethodHandle(J9VMThread *currentThread, j9object_t methodHandle, j9object_t arg, U_32 flags);
   void disableJit(J9JITConfig *jitConfig);
   void enableJit(J9JITConfig *jitConfig);

   jint onLoadInternal(J9JavaVM * javaVM, J9JITConfig *jitConfig, char *xjitCommandLine, char *xaotCommandLine, UDATA flagsParm, void *reserved0, I_32 xnojit);
   int32_t aboutToBootstrap(J9JavaVM * javaVM, J9JITConfig * jitConfig);
   void JitShutdown(J9JITConfig * jitConfig);
   void freeJITConfig(J9JITConfig * jitConfig);

#ifdef __cplusplus
}
#endif
#endif



