/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef JITPROTOS_H
#define JITPROTOS_H



#include "j9.h"
#include "j9cp.h"
#include "jitavl.h"




#ifdef __cplusplus
extern "C" {
#endif

/* prototypes from dlt.c */

void * setUpForDLT(J9VMThread * currentThread, J9StackWalkState * walkState);

/* prototypes from J9JITRTStub */

/* prototypes from JITSourceRuntimeInit */

/* prototypes from J9SourceJITRuntimeDecompile */
extern J9_CFUNC void   induceOSROnCurrentThread(J9VMThread * currentThread);
extern J9_CFUNC UDATA osrFrameSize(J9Method *method);
extern J9_CFUNC UDATA ensureOSRBufferSize(J9JavaVM *vm, UDATA osrFramesByteSize, UDATA osrScratchBufferByteSize, UDATA osrStackFrameByteSize);
extern J9_CFUNC void jitStackLocalsModified (J9VMThread * currentThread, J9StackWalkState * walkState);
#if (defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)) /* priv. proto (autogen) */
extern J9_CFUNC J9JITDecompilationInfo *
jitCleanUpDecompilationStack (J9VMThread * currentThread, J9StackWalkState * walkState, UDATA dropCurrentFrame);
#endif /* J9VM_INTERP_HOT_CODE_REPLACEMENT (autogen) */

extern J9_CFUNC void   jitDataBreakpointRemoved (J9VMThread * currentThread);
#if (defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)) /* priv. proto (autogen) */
extern J9_CFUNC void   jitDecompileMethodForFramePop (J9VMThread * currentThread, UDATA skipCount);
#endif /* J9VM_INTERP_HOT_CODE_REPLACEMENT (autogen) */

extern J9_CFUNC void   jitExceptionCaught (J9VMThread * currentThread);
extern J9_CFUNC void   jitSingleStepRemoved (J9VMThread * currentThread);
extern J9_CFUNC void   jitDataBreakpointAdded (J9VMThread * currentThread);
extern J9_CFUNC void  
jitBreakpointedMethodCompiled (J9VMThread * currentThread, J9Method * method, void * startAddress);
extern J9_CFUNC UDATA
initializeFSD (J9JavaVM * vm);
extern J9_CFUNC void 
induceOSROnCurrentThread(J9VMThread * currentThread);
#if (defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)) /* priv. proto (autogen) */
extern J9_CFUNC void   jitHotswapOccurred (J9VMThread * currentThread);
#endif /* J9VM_INTERP_HOT_CODE_REPLACEMENT (autogen) */

extern J9_CFUNC void   jitCodeBreakpointAdded (J9VMThread * currentThread, J9Method * method);
extern J9_CFUNC void   jitSingleStepAdded (J9VMThread * currentThread);
extern J9_CFUNC void   jitDecompileMethod (J9VMThread * currentThread, J9JITDecompilationInfo * decompRecord);
extern J9_CFUNC void   jitCodeBreakpointRemoved (J9VMThread * currentThread, J9Method * method);
extern J9_CFUNC UDATA  jitIsMethodBreakpointed(J9VMThread *currentThread, J9Method *method);

extern J9_CFUNC void   c_jitDecompileAfterAllocation(J9VMThread * currentThread);
extern J9_CFUNC void   c_jitDecompileAfterMonitorEnter(J9VMThread * currentThread);
extern J9_CFUNC void   c_jitDecompileAtCurrentPC(J9VMThread * currentThread);
extern J9_CFUNC void   c_jitDecompileAtExceptionCatch(J9VMThread * currentThread);
extern J9_CFUNC void   c_jitDecompileBeforeMethodMonitorEnter(J9VMThread * currentThread);
extern J9_CFUNC void   c_jitDecompileBeforeReportMethodEnter(J9VMThread * currentThread);
extern J9_CFUNC void   c_jitReportExceptionCatch(J9VMThread * currentThread);
extern J9_CFUNC void   c_jitDecompileOnReturn(J9VMThread * currentThread);

/* prototypes from JITSourceCodertThunk */
extern J9_CFUNC void * j9ThunkLookupNameAndSig (void * jitConfig, void *parm);

/* prototypes from JITSourceHashTableSupport */

/* prototypes from JITSourceRuntimeCacheSupport */
extern J9_CFUNC J9JITHashTable *avl_jit_artifact_insert_existing_table (J9AVLTree * tree, J9JITHashTable * hashTable);
extern J9_CFUNC J9AVLTree * jit_allocate_artifacts (J9PortLibrary * portLibrary);

/* prototypes from JITStackWalker */
extern J9_CFUNC UDATA  jitWalkStackFrames (J9StackWalkState *walkState);
extern J9_CFUNC J9JITExceptionTable * jitGetExceptionTableFromPC (J9VMThread * vmThread, UDATA jitPC);
extern J9_CFUNC UDATA  jitGetOwnedObjectMonitors(J9StackWalkState *state);
#if (defined(J9VM_INTERP_STACKWALK_TRACING)) /* priv. proto (autogen) */
extern J9_CFUNC void jitPrintRegisterMapArray (J9StackWalkState * walkState, char * description);
#endif /* J9VM_INTERP_STACKWALK_TRACING (autogen) */


/* prototypes from JITSourceArtifactSupport */
extern J9_CFUNC J9JITHashTable *jit_artifact_add_code_cache (J9PortLibrary * portLibrary, J9AVLTree * tree, J9MemorySegment * cacheToInsert, J9JITHashTable *optionalHashTable);
extern J9_CFUNC UDATA jit_artifact_insert (J9PortLibrary * portLibrary, J9AVLTree * tree, J9JITExceptionTable * dataToInsert);
extern J9_CFUNC J9JITHashTable *
jit_artifact_protected_add_code_cache (J9JavaVM * vm, J9AVLTree * tree, J9MemorySegment * cacheToInsert, J9JITHashTable *optionalHashTable);
extern J9_CFUNC UDATA jit_artifact_remove (J9PortLibrary * portLibrary, J9AVLTree * tree, J9JITExceptionTable * dataToDelete);

/* prototypes from thunkcrt.c */
void * j9ThunkLookupNameAndSig(void * jitConfig, void *parm);
UDATA j9ThunkTableAllocate(J9JavaVM * vm);
void j9ThunkTableFree(J9JavaVM * vm);
void * j9ThunkLookupSignature(void * jitConfig, UDATA signatureLength, char *signatureChars);
IDATA j9ThunkNewNameAndSig(void * jitConfig, void *parm, void *thunkAddress);
IDATA j9ThunkNewSignature(void * jitConfig, int signatureLength, char *signatureChars, void *thunkAddress);
void * j9ThunkVMHelperFromSignature(void * jitConfig, UDATA signatureLength, char *signatureChars);
void * j9ThunkInvokeExactHelperFromSignature(void * jitConfig, UDATA signatureLength, char *signatureChars);

/* prototypes from CodertVMHelpers.cpp */
void initializeDirectJNI (J9JavaVM *vm);
void flushICache(J9VMThread *currentThread, void *memoryPointer, UDATA byteAmount);

/* prototypes from jsr292.c */
void i2jFSDAssert();

/* prototype from cnathelp.cpp */

void initPureCFunctionTable(J9JavaVM *vm);

/**
 * Make the stack walkable by simulating a call to a resolve helper
 * (pushing a fake slot for the return address on platforms where the RA is pushed)
 * and building a resolve frame.
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] pc the JIT PC for the resolve frame
 * @param[in] flags the resolve frame flags
 */
void buildBranchJITResolveFrame(J9VMThread *currentThread, void *pc, UDATA flags);

/**
 * Collapse a resolve frame built by buildBranchJITResolveFrame.
 *
 * @param[in] currentThread the current J9VMThread
 */
void restoreBranchJITResolveFrame(J9VMThread *currentThread);

/* Fast path only C helpers to be called directly from the compiled code */

#if defined(J9VM_ARCH_X86) && !defined(J9VM_ENV_DATA64)
#if defined(WIN32)
#define J9FASTCALL __fastcall
#else /* WIN32 */
#define J9FASTCALL __attribute__((fastcall))
#endif /* WIN32 */
#else /* defined(J9VM_ARCH_X86) && !defined(J9VM_ENV_DATA64) */
#define J9FASTCALL
#endif /* defined(J9VM_ARCH_X86) && !defined(J9VM_ENV_DATA64) */

/* New-style helpers */

#if !defined(J9VM_ENV_DATA64)
U_64 J9FASTCALL fast_jitVolatileReadLong(J9VMThread *currentThread, U_64 *address);
void J9FASTCALL fast_jitVolatileWriteLong(J9VMThread *currentThread, U_64* address, U_64 value);
jdouble J9FASTCALL fast_jitVolatileReadDouble(J9VMThread *currentThread, U_64* address);
void J9FASTCALL fast_jitVolatileWriteDouble(J9VMThread *currentThread, U_64 *address, jdouble value);
#endif /* !J9VM_ENV_DATA64 */
void J9FASTCALL fast_jitCheckIfFinalizeObject(J9VMThread *currentThread, j9object_t object);
void J9FASTCALL fast_jitCollapseJNIReferenceFrame(J9VMThread *currentThread);
#if defined(J9VM_ARCH_X86) || defined(J9VM_ARCH_S390)
/* TODO Will be cleaned once all platforms adopt the correct parameter order */
UDATA J9FASTCALL fast_jitInstanceOf(J9VMThread *currentThread, j9object_t object, J9Class *castClass);
#else /* J9VM_ARCH_X86 || J9VM_ARCH_S390*/
UDATA J9FASTCALL fast_jitInstanceOf(J9VMThread *currentThread, J9Class *castClass, j9object_t object);
#endif /* J9VM_ARCH_X86 || J9VM_ARCH_S390*/
UDATA J9FASTCALL fast_jitObjectHashCode(J9VMThread *currentThread, j9object_t object);

#ifdef __cplusplus
}
#endif

#endif /* JITPROTOS_H */
