/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#ifndef __ROSSA_H
#define __ROSSA_H

/* Please insert new codes before compilationMaxError and ensure the
 * corresponding names are added in compilationErrorNames in rossa.cpp */
typedef enum {
   compilationOK                                        = 0,
   compilationFailure                                   = 1,  /* catch all error */
   compilationRestrictionILNodes                        = 2,  /* Unused */
   compilationRestrictionRecDepth                       = 3,  /* Unused */
   compilationRestrictedMethod                          = 4,  /* filters, JNI, abstract */
   compilationExcessiveComplexity                       = 5,
   compilationNotNeeded                                 = 6,
   compilationSuspended                                 = 7,
   compilationExcessiveSize                             = 8,  /* full caches */
   compilationInterrupted                               = 9,  /* GC wants to unload classes */
   compilationMetaDataFailure                           = 10, /* cannot create metadata */
   compilationInProgress                                = 11, /* for async compilations */
   compilationCHTableCommitFailure                      = 12,
   compilationMaxCallerIndexExceeded                    = 13,
   compilationKilledByClassReplacement                  = 14,
   compilationHeapLimitExceeded                         = 15,
   compilationNeededAtHigherLevel                       = 16, /*rtj */
   compilationAotTrampolineReloFailure                  = 17,
   compilationAotPicTrampolineReloFailure               = 18,
   compilationAotCacheFullReloFailure                   = 19,
   compilationCodeReservationFailure                    = 20,
   compilationAotHasInvokehandle                        = 21,
   compilationTrampolineFailure                         = 22,
   compilationRecoverableTrampolineFailure              = 23, /* we should retry these */
   compilationILGenFailure                              = 24,
   compilationIllegalCodeCacheSwitch                    = 25,
   compilationNullSubstituteCodeCache                   = 26,
   compilationCodeMemoryExhausted                       = 27,
   compilationGCRPatchFailure                           = 28,
   compilationLambdaEnforceScorching                    = 29,
   compilationInternalPointerExceedLimit                = 30,
   compilationAotRelocationInterrupted                  = 31,
   compilationAotClassChainPersistenceFailure           = 32,
   compilationLowPhysicalMemory                         = 33,
   compilationDataCacheError                            = 34,
   compilationCodeCacheError                            = 35,
   compilationRecoverableCodeCacheError                 = 36,
   compilationAotHasInvokeVarHandle                     = 37,
   compilationFSDHasInvokeHandle                        = 38,
   compilationVirtualAddressExhaustion                  = 39,
   compilationEnforceProfiling                          = 40,
   compilationSymbolValidationManagerFailure            = 41,
   compilationAOTNoSupportForAOTFailure                 = 42,
   compilationILGenUnsupportedValueTypeOperationFailure = 43,
   compilationAOTRelocationRecordGenerationFailure      = 44,
   compilationAotPatchedCPConstant                      = 45,
   compilationAotHasInvokeSpecialInterface              = 46,
   compilationRelocationFailure                         = 47,
   compilationAOTThunkPersistenceFailure                = 48,
#if defined(J9VM_OPT_JITSERVER)
   compilationFirstJITServerFailure,
   compilationStreamFailure                             = compilationFirstJITServerFailure,     // 49
   compilationStreamLostMessage                         = compilationFirstJITServerFailure + 1, // 50
   compilationStreamMessageTypeMismatch                 = compilationFirstJITServerFailure + 2, // 51
   compilationStreamVersionIncompatible                 = compilationFirstJITServerFailure + 3, // 52
   compilationStreamInterrupted                         = compilationFirstJITServerFailure + 4, // 53
   aotCacheDeserializationFailure                       = compilationFirstJITServerFailure + 5, // 54
   aotDeserializerReset                                 = compilationFirstJITServerFailure + 6, // 55
   compilationAOTCachePersistenceFailure                = compilationFirstJITServerFailure + 7, // 56
#endif /* defined(J9VM_OPT_JITSERVER) */

   /* must be the last one */
   compilationMaxError
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



