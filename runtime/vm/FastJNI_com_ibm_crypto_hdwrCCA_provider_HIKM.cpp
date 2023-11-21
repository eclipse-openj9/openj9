/*******************************************************************************
 * Copyright (c) 2022, 2024 IBM Corp. and others
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


#include "fastJNI.h"

#include "j9protos.h"
#include "j9consts.h"
#include "j9vmnls.h"
#include "objhelp.h"
#include "BytecodeAction.hpp"
#include "VMHelpers.hpp"
#include "ArrayCopyHelpers.hpp"
#include "ut_j9vm.h"

//#define CRYPTO_DEBUG 0

#define J9_CRYPTO_DLL_NAME (char*)"HIKM"

#if !defined(PATH_MAX)
/* This is a somewhat arbitrarily selected fixed buffer size. */
#define PATH_MAX 1024
#endif

/* Controls the maximum number of bytes that can be processed at a time before
VMAccess is released by the crypto AES, DES, and TripleDES algorithms */
#define CIPHER_OP_SIZE_LIMIT 49024

#define INITVECTOR 0
#define CHAINDATA  1
#define REGPATH    2

#define ENCRYPT 0
#define DECRYPT 1

#define AES 0
#define DES 1
#define GCM 2

#if defined(CRYPTO_DEBUG)
#define TRACE_ENTER() fprintf(stderr, "%s entered\n", __FUNCTION__)
#define TRACE_EXIT()  fprintf(stderr, "%s exiting\n", __FUNCTION__)
#define TRACE_EXIT_RC(_rc) fprintf(stderr, "%s exiting rc=%d\n", __FUNCTION__, _rc)
#define TRACE0(_fmtStr)  fprintf(stderr, _fmtStr)
#define TRACE1(_fmtStr, _p1) fprintf(stderr, _fmtStr, _p1)
#define TRACE2(_fmtStr, _p1, _p2) fprintf(stderr, _fmtStr, _p1, _p2)
#define TRACE3(_fmtStr, _p1, _p2, _p3) fprintf(stderr, _fmtStr, _p1, _p2, _p3)
#define TRACE4(_fmtStr, _p1, _p2, _p3, _p4) fprintf(stderr, _fmtStr, _p1, _p2, _p3, _p4)
#define TRACE5(_fmtStr, _p1, _p2, _p3, _p4, _p5) fprintf(stderr, _fmtStr, _p1, _p2, _p3, _p4, _p5)
#define TRACE6(_fmtStr, _p1, _p2, _p3, _p4, _p5, _p6) fprintf(stderr, _fmtStr, _p1, _p2, _p3, _p4, _p5, _p6)
#else /* defined(CRYPTO_DEBUG) */
#define TRACE_ENTER()
#define TRACE_EXIT()
#define TRACE_EXIT_RC(_rc)
#define TRACE0(_fmtStr)
#define TRACE1(_fmtStr, _p1)
#define TRACE2(_fmtStr, _p1, _p2)
#define TRACE3(_fmtStr, _p1, _p2, _p3)
#define TRACE4(_fmtStr, _p1, _p2, _p3, _p4)
#define TRACE5(_fmtStr, _p1, _p2, _p3, _p4, _p5)
#define TRACE6(_fmtStr, _p1, _p2, _p3, _p4, _p5, _p6)
#endif /* defined(CRYPTO_DEBUG) */

extern "C" {

/* The operations and request structure must be kept in sync with the definitions
 * in hikm.h and hikmimp.c operations.
 */
#define ONLY   1
#define FIRST  2
#define MIDDLE 3
#define LAST   4

/* request structure */
typedef struct {
    jbyte *key_identifier;
    jbyte *initialization_vector;
    jbyte *input;
    jbyte *AAD_data;
    jbyte *output;
    jbyte *chain_data;
    jbyte *AuthTag;
    int key_length;
    int initialization_vector_length;
    long total_length;
    long input_length;
    long AAD_data_length;
    int output_length;
    int chain_data_length;
    int authTagLen;
    int icvMode;
} cipher_request_t;

typedef int CIPHER_t(void * rule_array,
                     void * key_identifier,
                     void * initialization_vector,
                     void * clear_text,
                     void * cipher_text,
                     void * chain_data,
                     int return_code,
                     int reason_code,
                     int key_length,
                     int initialization_vector_length,
                     int clear_text_length,
                     int cipher_text_length,
                     int chain_data_length,
                     int rule_array_count,
                     int XOROperation);

typedef int GCM_CIPHER_t(cipher_request_t *pRequest);

typedef int SHA_t(void * ra,
                  void * text,
                  void * chaining_vector,
                  void * hash,
                  int rc,
                  int rea,
                  int ra_count,
                  int text_length,
                  int ra_length);

typedef union {
    CIPHER_t *CIPHER_op;
    GCM_CIPHER_t *GCM_CIPHER_op;
} cryptoFunc_t;

typedef union {
    SHA_t* SHA_op;
} SHAFunc_t;

typedef struct cryptoPointers {
    cryptoFunc_t KMAESEPtr;
    cryptoFunc_t KMAESDPtr;
    cryptoFunc_t KMDESEPtr;
    cryptoFunc_t KMDESDPtr;
    cryptoFunc_t KMGCMEPtr;
    cryptoFunc_t KMGCMDPtr;
    SHAFunc_t KXMDSHAPtr;
    SHAFunc_t KXMDSHA224Ptr;
    SHAFunc_t KXMDSHA256Ptr;
    SHAFunc_t KXMDSHA384Ptr;
    SHAFunc_t KXMDSHA512Ptr;
    CIPHER_t* KMAESE;
    CIPHER_t* KMAESD;
    CIPHER_t* KMDESE;
    CIPHER_t* KMDESD;
    GCM_CIPHER_t* KMGCME;
    GCM_CIPHER_t* KMGCMD;
    SHA_t* KXMDSHA;
    SHA_t* KXMDSHA224;
    SHA_t* KXMDSHA256;
    SHA_t* KXMDSHA384;
    SHA_t* KXMDSHA512;
} cryptoPointers_t;

/*
 * loads crypto lib routines and store routine pointers for use.
 * Need to synchronize this operation so that threads won't
 * use the routines too early
 *
 * @param[in] currentThread - current thread
 */
static void
loadCryptoLib(J9VMThread *currentThread)
{
    TRACE_ENTER();

    omrthread_monitor_t mutex = currentThread->javaVM->jniHikmCryptoLibLock;
    omrthread_monitor_enter(mutex);
    if (NULL == currentThread->javaVM->jniHikmCryptoFunctions) {
        PORT_ACCESS_FROM_VMC(currentThread);
        cryptoPointers_t *cryptFuncs = (cryptoPointers_t*)j9mem_allocate_memory(sizeof(cryptoPointers_t), J9MEM_CATEGORY_VM_JCL);
        UDATA* cryptoLibrary = &(currentThread->javaVM->jniHikmCryptoLibrary);
        J9PortLibrary * portLib = currentThread->javaVM->portLibrary;
        char* dir = currentThread->javaVM->j2seRootDirectory;
        char correctPath[PATH_MAX] = "";
        char *correctPathPtr = correctPath;

        if (NULL == cryptFuncs) {
            setNativeOutOfMemoryError(currentThread, J9NLS_VM_NATIVE_OOM);
        } else {
            /* Attempting to load the crypto library and needed functions
             * vm assert if the library does not load correctly
             * the crypto library is part of the jdk and a incorrect loading
             * indicates a corruption or a bug in the jdk */
            if (NULL != dir) {
                /* expectedPathLength - %s/../%s - +3 includes .. and NUL terminator */
                UDATA expectedPathLength = strlen(dir) + strlen(J9_CRYPTO_DLL_NAME) + (strlen(DIR_SEPARATOR_STR) * 2) + 3;
                if (expectedPathLength > PATH_MAX) {
                    correctPathPtr = (char*)j9mem_allocate_memory(expectedPathLength, J9MEM_CATEGORY_VM_JCL);
                    if (NULL == correctPathPtr) {
                        setNativeOutOfMemoryError(currentThread, J9NLS_VM_NATIVE_OOM);
                    }
                }
                if (NULL != correctPathPtr) {
                    j9str_printf(portLib, correctPathPtr, expectedPathLength, "%s%s..%s%s", dir, DIR_SEPARATOR_STR, DIR_SEPARATOR_STR, J9_CRYPTO_DLL_NAME);
#if defined(CRYPTO_DEBUG)
                    fprintf(stderr,"** Attempting to load crypto lib from %s\n", correctPathPtr);
#endif /* CRYPTO_DEBUG */
                    if (0 != j9sl_open_shared_library(correctPathPtr, cryptoLibrary, J9PORT_SLOPEN_DECORATE)) {
                        /* Attempt to load crypto, relying on LIBPATH */
                        if (0 != j9sl_open_shared_library(J9_CRYPTO_DLL_NAME, cryptoLibrary, J9PORT_SLOPEN_DECORATE)) {
                            Assert_VM_internalError();
                        }
                    }
                }
            } else {
                /* dir is NULL. It shouldn't happen, but in case, revert back to original dlopen that
                 * replies on LIBPATH */
                if (0 != j9sl_open_shared_library(J9_CRYPTO_DLL_NAME, cryptoLibrary, J9PORT_SLOPEN_DECORATE)) {
                    Assert_VM_internalError();
                }
            }

            if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"KMAESE_internal", (UDATA*)&(cryptFuncs->KMAESE), "iLLLLLLiiiiiiiii")) {
                Assert_VM_internalError();
            }
            if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"KMAESD_internal", (UDATA*)&(cryptFuncs->KMAESD), "iLLLLLLiiiiiiiii")) {
                Assert_VM_internalError();
            }
            if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"KMDESE_internal", (UDATA*)&(cryptFuncs->KMDESE), "iLLLLLLiiiiiiiii")) {
                Assert_VM_internalError();
            }
            if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"KMDESD_internal", (UDATA*)&(cryptFuncs->KMDESD), "iLLLLLLiiiiiiiii")) {
                Assert_VM_internalError();
            }
            if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"KMGCME_internal", (UDATA*)&(cryptFuncs->KMGCME), "iJ")) {
                Assert_VM_internalError();
            }
            if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"KMGCMD_internal", (UDATA*)&(cryptFuncs->KMGCMD), "iJ")) {
                Assert_VM_internalError();
            }
            if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"KXMDSHA_internal", (UDATA*)&(cryptFuncs->KXMDSHA), "iLLLLiiiii")) {
                Assert_VM_internalError();
            }
            if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"KXMDSHA224_internal", (UDATA*)&(cryptFuncs->KXMDSHA224), "iLLLLiiiii")) {
                Assert_VM_internalError();
            }
            if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"KXMDSHA256_internal", (UDATA*)&(cryptFuncs->KXMDSHA256), "iLLLLiiiii")) {
                Assert_VM_internalError();
            }
            if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"KXMDSHA384_internal", (UDATA*)&(cryptFuncs->KXMDSHA384), "iLLLLiiiii")) {
                Assert_VM_internalError();
            }
            if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"KXMDSHA512_internal", (UDATA*)&(cryptFuncs->KXMDSHA512), "iLLLLiiiii")) {
                Assert_VM_internalError();
            }

#if defined(CRYPTO_DEBUG)
            printf("** KMAESE: %p\n", cryptFuncs->KMAESE);
            printf("** KMAESD: %p\n", cryptFuncs->KMAESD);
            printf("** KMDESE: %p\n", cryptFuncs->KMDESE);
            printf("** KMDESD: %p\n", cryptFuncs->KMDESD);
            printf("** KMGCME: %p\n", cryptFuncs->KMGCME);
            printf("** KMGCMD: %p\n", cryptFuncs->KMGCMD);
            printf("** KXMDSHA: %p\n", cryptFuncs->KXMDSHA);
            printf("** KXMDSHA224: %p\n", cryptFuncs->KXMDSHA224);
            printf("** KXMDSHA256: %p\n", cryptFuncs->KXMDSHA256);
            printf("** KXMDSHA384: %p\n", cryptFuncs->KXMDSHA384);
            printf("** KXMDSHA512: %p\n", cryptFuncs->KXMDSHA512);
#endif /* CRYPTO_DEBUG */

            cryptFuncs->KMAESEPtr.CIPHER_op      = cryptFuncs->KMAESE;
            cryptFuncs->KMAESDPtr.CIPHER_op      = cryptFuncs->KMAESD;
            cryptFuncs->KMDESEPtr.CIPHER_op      = cryptFuncs->KMDESE;
            cryptFuncs->KMDESDPtr.CIPHER_op      = cryptFuncs->KMDESD;
            cryptFuncs->KMGCMEPtr.GCM_CIPHER_op  = cryptFuncs->KMGCME;
            cryptFuncs->KMGCMDPtr.GCM_CIPHER_op  = cryptFuncs->KMGCMD;
            cryptFuncs->KXMDSHAPtr.SHA_op        = cryptFuncs->KXMDSHA;
            cryptFuncs->KXMDSHA224Ptr.SHA_op     = cryptFuncs->KXMDSHA224;
            cryptFuncs->KXMDSHA256Ptr.SHA_op     = cryptFuncs->KXMDSHA256;
            cryptFuncs->KXMDSHA384Ptr.SHA_op     = cryptFuncs->KXMDSHA384;
            cryptFuncs->KXMDSHA512Ptr.SHA_op     = cryptFuncs->KXMDSHA512;
            currentThread->javaVM->jniHikmCryptoFunctions = (void*)cryptFuncs;
            if ((correctPath != correctPathPtr) && (NULL != correctPathPtr)) {
                j9mem_free_memory(correctPathPtr);
            }
        }
    }
    omrthread_monitor_exit(mutex);
    TRACE_EXIT();
}

/*
 * This function was copied from VM_ArrayCopyHelpers class where it is marked as private.
 * The original function works for any element size. This function was specialized to use elementSize of 1
 * since we are only working with bytes.
 *
 * @param[in] currentThread - current thread
 * @param[in] index - the starting index into the array
 * @param[in] count - length of the range
 *
 * @return boolean indicating if Array is contiguous or not at that range
 */
static VMINLINE bool
isCryptoArrayContiguous(J9VMThread *currentThread, UDATA index, UDATA count)
{
    UDATA arrayletLeafSizeInElements = currentThread->javaVM->arrayletLeafSize;
    UDATA lastIndex = index;

    lastIndex += count;
    lastIndex -= 1;
    lastIndex ^= index;
    return lastIndex < arrayletLeafSizeInElements;
}

static VMINLINE jint
cipherLaunch(J9VMThread *currentThread, jint return_code, jint reason_code, jint rule_array_count, j9object_t rule_array, jint key_length, j9object_t key_identifier, jint initialization_vector_length, j9object_t initialization_vector, jint chain_data_length, j9object_t chain_data, jint input_length, j9object_t input, jint output_length, j9object_t output, CIPHER_t* cbcOpUpdate, cryptoFunc_t cbcOp, jlong paramPointer, jint mode, jint algorithm)
{
#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Enter cipherLaunch %p %p %p %p %p %p ---- \n", rule_array, key_identifier, initialization_vector, chain_data, input, output);
#endif /* CRYPTO_DEBUG */

    jint totalBytes = 0;
    /* The DES and TripleDES arrays come in with the inputs and outputs already
     * switched, but the output_length needs to be used for AES to get the length
     * of the cleartext for decryption, or else the totalBytes will be too large
     * and could cause an error dump during execution
     */
    if ((AES == algorithm) && (DECRYPT == mode)) {
        totalBytes = output_length;
    } else {
        totalBytes = input_length;
    }

    jint result = 0;
    int inputIndex = 0;
    int outputIndex = 0;

    j9object_t ruleArrayObj     = J9_JNI_UNWRAP_REFERENCE(rule_array);
    j9object_t keyIdentifierObj = J9_JNI_UNWRAP_REFERENCE(key_identifier);
    j9object_t initVectorObj    = J9_JNI_UNWRAP_REFERENCE(initialization_vector);
    j9object_t chainDataObj     = J9_JNI_UNWRAP_REFERENCE(chain_data);
    j9object_t inputObj         = J9_JNI_UNWRAP_REFERENCE(input);
    j9object_t outputObj        = J9_JNI_UNWRAP_REFERENCE(output);

    void* ruleArrayAddress      = J9JAVAARRAY_EA(currentThread, ruleArrayObj, 0, U_8);
    void* keyIdentifierAddress  = J9JAVAARRAY_EA(currentThread, keyIdentifierObj, 0, U_8);
    void* initVectorAddress     = J9JAVAARRAY_EA(currentThread, initVectorObj, 0, U_8);
    void* chainDataAddress      = J9JAVAARRAY_EA(currentThread, chainDataObj, 0, U_8);
    void* inputAddress          = J9JAVAARRAY_EA(currentThread, inputObj, inputIndex, U_8);
    void* outputAddress         = J9JAVAARRAY_EA(currentThread, outputObj, outputIndex, U_8);

    if (totalBytes <= CIPHER_OP_SIZE_LIMIT) {
        /* The size of the totalBytes is not too large and can be done with a
         * single call
         */
        result = (*(cbcOp.CIPHER_op))(ruleArrayAddress,
                                      keyIdentifierAddress,
                                      initVectorAddress,
                                      inputAddress,
                                      outputAddress,
                                      chainDataAddress,
                                      return_code,
                                      reason_code,
                                      key_length,
                                      initialization_vector_length,
                                      input_length,
                                      output_length,
                                      chain_data_length,
                                      rule_array_count,
                                      REGPATH);

    } else {
        /* The requested operation is too large to be done in a single op since
         * that would acquire VMAccess for too long, so the operation is broken
         * up
         */
        int returnCode = 0;
        jint currentBytes = 0;
        jint bytesToOp = totalBytes;

        while (0 != bytesToOp) {
            if ((bytesToOp - CIPHER_OP_SIZE_LIMIT) >= 0) {
                currentBytes = CIPHER_OP_SIZE_LIMIT;
            } else {
                currentBytes = bytesToOp;
            }

            if (bytesToOp == totalBytes) {
                /* If the first call, we need to tell IBMJCECCA to use the
                 * initialization vector when encrypting and decrypting the byte
                 * arrays.
                 */
#if defined(CRYPTO_DEBUG)
                fprintf(stderr,"cipherLaunch, first call\n");
#endif /* CRYPTO_DEBUG */
                returnCode = (*(cbcOp.CIPHER_op))(
                                    ruleArrayAddress,
                                    keyIdentifierAddress,
                                    initVectorAddress,
                                    inputAddress,
                                    outputAddress,
                                    chainDataAddress,
                                    return_code,
                                    reason_code,
                                    key_length,
                                    initialization_vector_length,
                                    currentBytes,
                                    currentBytes,
                                    chain_data_length,
                                    rule_array_count,
                                    INITVECTOR);

            } else if ((bytesToOp - currentBytes) > 0) {
                /* Subsequent but not final call. For subsequent calls, we will
                 * tell IBMJCECCA to use the chain data for the rest of the
                 * operation
                 */
#if defined(CRYPTO_DEBUG)
                fprintf(stderr,"cipherLaunch, subsequent but not final call\n");
#endif /* CRYPTO_DEBUG */
                returnCode = (*(cbcOp.CIPHER_op))(
                                    ruleArrayAddress,
                                    keyIdentifierAddress,
                                    initVectorAddress,
                                    inputAddress,
                                    outputAddress,
                                    chainDataAddress,
                                    return_code,
                                    reason_code,
                                    key_length,
                                    initialization_vector_length,
                                    currentBytes,
                                    currentBytes,
                                    chain_data_length,
                                    rule_array_count,
                                    CHAINDATA);
            } else {
                /* Final call */
#if defined(CRYPTO_DEBUG)
                fprintf(stderr,"cipherLaunch, final call\n");
#endif /* CRYPTO_DEBUG */
                returnCode = (*(cbcOp.CIPHER_op))(
                                    ruleArrayAddress,
                                    keyIdentifierAddress,
                                    initVectorAddress,
                                    inputAddress,
                                    outputAddress,
                                    chainDataAddress,
                                    return_code,
                                    reason_code,
                                    key_length,
                                    initialization_vector_length,
                                    currentBytes,
                                    currentBytes,
                                    chain_data_length,
                                    rule_array_count,
                                    CHAINDATA);
                /* Operation done, return */
                if (returnCode < 0) {
                    result = returnCode;
                } else {
                    result = (result + returnCode);
                }
                break;
            }

            if (returnCode < 0) {
                /* Crypto returned error, returning immediately */
                result = returnCode;
                break;
            }
            /* No error, crypto returns length. */
            result += returnCode;

            /* Release VMAccess to allow GC to work if needed. Recover
             * input/output pointers afterwards
             */
            if (J9_ARE_ANY_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_ANY)) {
                J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;
                vmFuncs->internalReleaseVMAccess(currentThread);
                vmFuncs->internalAcquireVMAccess(currentThread);
                inputObj = J9_JNI_UNWRAP_REFERENCE(input);
                outputObj = J9_JNI_UNWRAP_REFERENCE(output);
            }
            inputIndex += currentBytes;
            outputIndex += returnCode;
            inputAddress = J9JAVAARRAY_EA(currentThread, inputObj, inputIndex, U_8);
            outputAddress = J9JAVAARRAY_EA(currentThread, outputObj, outputIndex, U_8);
            bytesToOp -= currentBytes;
        }
    }

#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Exit cipherLaunch\n");
#endif /* CRYPTO_DEBUG */

    return result;
}

static VMINLINE jint
discontiguousCipherLaunch(J9VMThread *currentThread, jint return_code, jint reason_code, jint rule_array_count, j9object_t rule_array, jint key_length, j9object_t key_identifier, jint initialization_vector_length, j9object_t initialization_vector, jint chain_data_length, j9object_t chain_data, jint input_length, j9object_t input, jint output_length, j9object_t output, CIPHER_t* cbcOpUpdate, cryptoFunc_t cbcOp, jlong paramPointer, jint mode, jint algorithm)
{
#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Enter discontiguousCipherLaunch %p %p %p %p %p %p ---- \n", rule_array, key_identifier, initialization_vector, chain_data, input, output);
#endif /* CRYPTO_DEBUG */

    jint ret = 0;
    int offset = 0;
    PORT_ACCESS_FROM_VMC(currentThread);

    j9object_t ruleArrayObj     = J9_JNI_UNWRAP_REFERENCE(rule_array);
    j9object_t keyIdentifierObj = J9_JNI_UNWRAP_REFERENCE(key_identifier);
    j9object_t initVectorObj    = J9_JNI_UNWRAP_REFERENCE(initialization_vector);
    j9object_t chainDataObj     = J9_JNI_UNWRAP_REFERENCE(chain_data);

    void* ruleArrayAddress      = J9JAVAARRAY_EA(currentThread, ruleArrayObj, 0, U_8);
    void* keyIdentifierAddress  = J9JAVAARRAY_EA(currentThread, keyIdentifierObj, 0, U_8);
    void* initVectorAddress     = J9JAVAARRAY_EA(currentThread, initVectorObj, 0, U_8);
    void* chainDataAddress      = J9JAVAARRAY_EA(currentThread, chainDataObj, 0, U_8);

    /* Allocate native buffers to be used to directly interact with crypto API */
    void* inputNativePtr        = j9mem_allocate_memory(input_length, J9MEM_CATEGORY_VM_JCL);
    void* outputNativePtr       = j9mem_allocate_memory(output_length, J9MEM_CATEGORY_VM_JCL);

    if ((NULL != outputNativePtr) && (NULL != inputNativePtr)) {

        /* Copy from arraylets to native buffer */
        if ((AES == algorithm) && (DECRYPT == mode)) {
            VM_ArrayCopyHelpers::memcpyFromArray(currentThread, J9_JNI_UNWRAP_REFERENCE(output), offset, output_length, outputNativePtr);
        } else {
            VM_ArrayCopyHelpers::memcpyFromArray(currentThread, J9_JNI_UNWRAP_REFERENCE(input), offset, input_length, inputNativePtr);
        }

        /* Do crypto operation */
        ret = (*(cbcOp.CIPHER_op))(ruleArrayAddress,
                                   keyIdentifierAddress,
                                   initVectorAddress,
                                   inputNativePtr,
                                   outputNativePtr,
                                   chainDataAddress,
                                   return_code,
                                   reason_code,
                                   key_length,
                                   initialization_vector_length,
                                   input_length,
                                   output_length,
                                   chain_data_length,
                                   rule_array_count,
                                   REGPATH);

        /* Negative ret means an error code, don't copy in that case */
        if (ret >= 0) {
            /* Copy back from output native buffer to output arraylets */
            if ((AES == algorithm) && (DECRYPT == mode)) {
                VM_ArrayCopyHelpers::memcpyToArray(currentThread, J9_JNI_UNWRAP_REFERENCE(input), offset, ret, inputNativePtr);
            } else {
                VM_ArrayCopyHelpers::memcpyToArray(currentThread, J9_JNI_UNWRAP_REFERENCE(output), offset, ret, outputNativePtr);
            }
        }

    } else {
        /* Throw OutOfMemory exception, unable to allocate buffer */
        ret = -1;
        setNativeOutOfMemoryError(currentThread, J9NLS_VM_NATIVE_OOM);
    }

    /* Release input and output native buffers. */
    j9mem_free_memory(inputNativePtr);
    j9mem_free_memory(outputNativePtr);
#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Exit discontiguousCipherLaunch\n");
#endif /* CRYPTO_DEBUG */

    /* Return output size or error code */
    return ret;
}

static VMINLINE jint
aesCipherLaunch(J9VMThread       *currentThread,
                j9object_t        input,
                j9object_t        output,
                cipher_request_t *pRequest,
                cryptoFunc_t      cbcOp)
{
    TRACE_ENTER();

    jint result = 0;
    int inputIndex = 0;
    int outputIndex = 0;

    j9object_t inputObj  = J9_JNI_UNWRAP_REFERENCE(input);
    j9object_t outputObj = J9_JNI_UNWRAP_REFERENCE(output);

    pRequest->input  = (jbyte*)J9JAVAARRAY_EA(currentThread, inputObj, inputIndex, U_8);
    pRequest->output = (jbyte*)J9JAVAARRAY_EA(currentThread, outputObj, outputIndex, U_8);

    if (pRequest->input_length <= CIPHER_OP_SIZE_LIMIT) {
        /* The size of pRequest->input_length is not too large and can be done with a
         * single call.
         */
         result = (*(cbcOp.GCM_CIPHER_op))(pRequest);

    } else {
        /* The requested operation is too large to be done in a single op since
         * that would acquire VMAccess for too long, so the operation is broken
         * up.
         */
        int returnCode = 0;
        jint currentBytes = 0;
        jint bytesToOp = pRequest->input_length;

        while (0 != bytesToOp) {
            if ((bytesToOp - CIPHER_OP_SIZE_LIMIT) >= 0) {
                currentBytes = CIPHER_OP_SIZE_LIMIT;
            } else {
                currentBytes = bytesToOp;
            }

            pRequest->input_length = currentBytes;
            pRequest->output_length = currentBytes;
            if (inputIndex == 0) {
                /* If the first call, we need to tell IBMJCECCA to use the
                 * initialization vector when encrypting and decrypting the byte
                 * arrays.
                 */
                TRACE0("aesCipherLaunch, first call\n");
                pRequest->icvMode = FIRST;
                returnCode = (*(cbcOp.GCM_CIPHER_op))(pRequest);

            } else if ((bytesToOp - currentBytes) > 0) {
                /* Subsequent but not final call. For subsequent calls, we will
                 * tell IBMJCECCA to use the chain data for the rest of the
                 * operation.
                 */
                TRACE0("aesCipherLaunch, subsequent but not final call\n");
                pRequest->icvMode = MIDDLE;
                returnCode = (*(cbcOp.GCM_CIPHER_op))(pRequest);

            } else {
                /* Final call */
                TRACE0("aesCipherLaunch, final call\n");
                pRequest->icvMode = LAST;
                returnCode = (*(cbcOp.GCM_CIPHER_op))(pRequest);
                break;
            }

            if (returnCode < 0) {
                /* Crypto returned error, returning immediately. */
                result = returnCode;
                break;
            }
            /* No error, crypto returns length. */
            result += returnCode;

            /* Release VMAccess to allow GC to work if needed. Recover
             * input/output pointers afterwards.
             */
            if (J9_ARE_ANY_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_ANY)) {
                J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;
                vmFuncs->internalReleaseVMAccess(currentThread);
                vmFuncs->internalAcquireVMAccess(currentThread);
                inputObj = J9_JNI_UNWRAP_REFERENCE(input);
                outputObj = J9_JNI_UNWRAP_REFERENCE(output);
            }
            inputIndex += currentBytes;
            outputIndex += returnCode;
            pRequest->input = (jbyte*)J9JAVAARRAY_EA(currentThread, inputObj, inputIndex, U_8);
            pRequest->output = (jbyte*)J9JAVAARRAY_EA(currentThread, outputObj, outputIndex, U_8);
            bytesToOp -= currentBytes;
        }
    }

    TRACE_EXIT_RC(result);
    return result;
}


static VMINLINE jint
aesDiscontiguousCipherLaunch(J9VMThread       *currentThread,
                             j9object_t        input,
                             j9object_t        output,
                             cipher_request_t *pRequest,
                             cryptoFunc_t      cbcOp)
{
    TRACE_ENTER();

    jint ret = 0;
    int offset = 0;
    PORT_ACCESS_FROM_VMC(currentThread);

    /* Allocate native buffers to be used to directly interact with crypto API. */
    void* inputNativePtr  = j9mem_allocate_memory(pRequest->input_length, J9MEM_CATEGORY_VM_JCL);
    void* outputNativePtr = j9mem_allocate_memory(pRequest->output_length, J9MEM_CATEGORY_VM_JCL);

    if ((NULL != outputNativePtr) && (NULL != inputNativePtr)) {

        VM_ArrayCopyHelpers::memcpyFromArray(currentThread, J9_JNI_UNWRAP_REFERENCE(input), offset, pRequest->input_length, inputNativePtr);

        /* Do crypto operation. */
        pRequest->input = (signed char*)inputNativePtr;
        pRequest->output = (signed char*)outputNativePtr;
        ret = (*(cbcOp.GCM_CIPHER_op))(pRequest);

        /* Negative ret means an error code, don't copy in that case. */
        if (ret >= 0) {
            VM_ArrayCopyHelpers::memcpyToArray(currentThread, J9_JNI_UNWRAP_REFERENCE(output), offset, ret, outputNativePtr);
        }

    } else {
        /* Throw OutOfMemory exception, unable to allocate buffer. */
        ret = -1;
        setNativeOutOfMemoryError(currentThread, J9NLS_VM_NATIVE_OOM);
    }

    /* Release input and output native buffers. */
    j9mem_free_memory(inputNativePtr);
    j9mem_free_memory(outputNativePtr);

    /* Return output size or error code. */
    TRACE_EXIT_RC(ret);
    return ret;
}

static VMINLINE jint
cipher(J9VMThread *currentThread, jint return_code, jint reason_code, jint rule_array_count, j9object_t rule_array, jint key_length, j9object_t key_identifier, jint initialization_vector_length, j9object_t initialization_vector, jint chain_data_length, j9object_t chain_data, jint input_length, j9object_t input, jint output_length, j9object_t output, CIPHER_t* cbcOpUpdate, cryptoFunc_t cbcOp, jlong paramPointer, jint mode, jint algorithm)
{
#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Enter cipher %p %p %p %p %p %p ---- \n", rule_array, key_identifier, initialization_vector, chain_data, input, output);
#endif /* CRYPTO_DEBUG */

    jint offset = 0;
    jint result = 0;

    /* The if statement below determines if contiguous array or discontiguous array processing is required.
     * there are four inputs:
     *            1. is input array contiguous
     *            2. is output array contiguous
     *            3. is input length greater than zero
     *            4. is output length greater than zero
     * Note that the input length is passed from the JCL code so incorrect value
     * could cause a crash in the crypto lib - in which case the JCL code needs
     * to be fixed.
     */
    bool isContInput  = isCryptoArrayContiguous(currentThread, offset, input_length);
    bool isContOutput = isCryptoArrayContiguous(currentThread, offset, output_length);
    bool isContiguous = ((((  input_length >  0) && (output_length >  0)) && (isContInput && isContOutput))
                        || (((input_length <= 0) && (output_length >  0)) && isContOutput)
                        || (((input_length >  0) && (output_length <= 0)) && isContInput)
                        || (((input_length <= 0) && (output_length <= 0))));

    if (isContiguous) {
        /* Contiguous arrays */
#if defined(CRYPTO_DEBUG)
        fprintf(stderr,"cipher, The array is contiguous\n");
#endif /* CRYPTO_DEBUG */

        result = cipherLaunch(currentThread,
                              return_code,
                              reason_code,
                              rule_array_count,
                              rule_array,
                              key_length,
                              key_identifier,
                              initialization_vector_length,
                              initialization_vector,
                              chain_data_length,
                              chain_data,
                              input_length,
                              input,
                              output_length,
                              output,
                              cbcOpUpdate,
                              cbcOp,
                              paramPointer,
                              mode,
                              algorithm);
    } else {
        /* Discontiguous arrays */
#if defined(CRYPTO_DEBUG)
        fprintf(stderr,"cipher, The array is discontiguous\n");
#endif /* CRYPTO_DEBUG */

        result = discontiguousCipherLaunch(currentThread,
                                           return_code,
                                           reason_code,
                                           rule_array_count,
                                           rule_array,
                                           key_length,
                                           key_identifier,
                                           initialization_vector_length,
                                           initialization_vector,
                                           chain_data_length,
                                           chain_data,
                                           input_length,
                                           input,
                                           output_length,
                                           output,
                                           cbcOpUpdate,
                                           cbcOp,
                                           paramPointer,
                                           mode,
                                           algorithm);
    }

#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Exit cipher\n");
#endif /* CRYPTO_DEBUG */

    return result;
}

static VMINLINE jint
aesCipher(J9VMThread       *currentThread,
          j9object_t        input,
          j9object_t        output,
          cipher_request_t *pRequest,
          cryptoFunc_t      cbcOp)
{
    TRACE_ENTER();

    jint offset = 0;
    jint result = 0;

    /* The if statement below determines if contiguous array or discontiguous array
     * processing is required.
     * There are four inputs:
     *            1. is input array contiguous
     *            2. is output array contiguous
     *            3. is input length greater than zero
     *            4. is output length greater than zero
     * Note that the input length is passed from the JCL code so incorrect value
     * could cause a crash in the crypto lib - in which case the JCL code needs
     * to be fixed.
     */
    bool isContInput  = isCryptoArrayContiguous(currentThread, offset, pRequest->input_length);
    bool isContOutput = isCryptoArrayContiguous(currentThread, offset, pRequest->output_length);
    bool isContiguous = ((((  pRequest->input_length >  0) && (pRequest->output_length >  0)) && (isContInput && isContOutput))
                        || (((pRequest->input_length <= 0) && (pRequest->output_length >  0)) && isContOutput)
                        || (((pRequest->input_length >  0) && (pRequest->output_length <= 0)) && isContInput)
                        || (((pRequest->input_length <= 0) && (pRequest->output_length <= 0))));

    if (isContiguous) {
        TRACE0("cipher, The array is contiguous\n");

        result = aesCipherLaunch(currentThread,
                                 input,
                                 output,
                                 pRequest,
                                 cbcOp);
    } else {
        TRACE0("cipher, The array is discontiguous\n");

        result = aesDiscontiguousCipherLaunch(currentThread,
                                              input,
                                              output,
                                              pRequest,
                                              cbcOp);
    }

    TRACE_EXIT_RC(result);
    return result;
}

static VMINLINE jint
digest(J9VMThread *currentThread, jint return_code, jint reason_code, jint rule_array_count, j9object_t rule_array, jint rule_array_length, jint text_length, j9object_t text, jint chaining_vector_length, j9object_t chaining_vector, jint hash_length, j9object_t hash, SHA_t* cbcOpUpdate, SHAFunc_t cbcOp, jlong paramPointer, jint mode)
{

#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Enter digest %p %p %p %p ---- \n", rule_array, text, chaining_vector, hash);
#endif /* CRYPTO_DEBUG */

    int offset = 0;
    bool inputIsCont = isCryptoArrayContiguous(currentThread, offset, text_length);
    PORT_ACCESS_FROM_VMC(currentThread);
    jint result = -1;

    /* The if statement below determines if contiguous array or discontiguous
     * array processing is required. Note that the input length is passed from
     * the JCL code so incorrect value could cause a crash in the crypto. lib - in
     * which case the JCL code needs to be fixed.
     *
     * Although a discontiguous array path is handled with this if statement,
     * IBMJCECCA uses Java code to break up SHA operations into blocks no larger
     * than 2048 bytes. In order for an array to be discontiguous, the length must
     * be larger than the region size (512KB) in the GC. Because the maximum array
     * size is 2048 bytes, a discontiguous path would never be reached.
     */
    bool isContiguous = ((((text_length > 0)) & (inputIsCont)) || (text_length <= 0));

    if (isContiguous) {
        /* Contiguous arrays */
#if defined(CRYPTO_DEBUG)
        fprintf(stderr,"digest, The array is contiguous\n");
#endif /* CRYPTO_DEBUG */

        j9object_t ruleArrayObj      = J9_JNI_UNWRAP_REFERENCE(rule_array);
        j9object_t textObj           = J9_JNI_UNWRAP_REFERENCE(text);
        j9object_t chainingVectorObj = J9_JNI_UNWRAP_REFERENCE(chaining_vector);
        j9object_t hashObj           = J9_JNI_UNWRAP_REFERENCE(hash);

        void* ruleArrayAddress       = J9JAVAARRAY_EA(currentThread, ruleArrayObj, offset, U_8);
        void* textAddress            = J9JAVAARRAY_EA(currentThread, textObj, offset, U_8);
        void* chainingVectorAddress  = J9JAVAARRAY_EA(currentThread, chainingVectorObj, offset, U_8);
        void* hashAddress            = J9JAVAARRAY_EA(currentThread, hashObj, offset, U_8);

        /* At this point, the size of text_length would normally be checked to
         * ensure that the requested operation is not too large to be done in a
         * single op since that would acquire VMAccess for too long. However,
         * the text_length is already capped by the time it gets to this FastJNI
         * function. See com.ibm.crypto.hdwrCCA.provider.SHACommon for more details
         */
        result = (*(cbcOp.SHA_op))(ruleArrayAddress,
                                   textAddress,
                                   chainingVectorAddress,
                                   hashAddress,
                                   return_code,
                                   reason_code,
                                   rule_array_count,
                                   text_length,
                                   rule_array_length);

    } else {
        /* Discontiguous arrays */
#if defined(CRYPTO_DEBUG)
        fprintf(stderr,"digest, The array is discontiguous\n");
#endif /* CRYPTO_DEBUG */

        /* Allocate native buffers to be used to directly interact with crypto API */
        void* ruleArrayNativePtr      = j9mem_allocate_memory(rule_array_length, J9MEM_CATEGORY_VM_JCL);
        void* textNativePtr           = j9mem_allocate_memory(text_length, J9MEM_CATEGORY_VM_JCL);
        void* chainingVectorNativePtr = j9mem_allocate_memory(chaining_vector_length, J9MEM_CATEGORY_VM_JCL);
        void* hashNativePtr           = j9mem_allocate_memory(hash_length, J9MEM_CATEGORY_VM_JCL);

        if ((NULL != ruleArrayNativePtr)      && (NULL != textNativePtr) &&
            (NULL != chainingVectorNativePtr) && (NULL != hashNativePtr)) {

            /* Copy from input arraylets to native buffer */
            VM_ArrayCopyHelpers::memcpyFromArray(currentThread, J9_JNI_UNWRAP_REFERENCE(rule_array), offset, rule_array_length, ruleArrayNativePtr);
            VM_ArrayCopyHelpers::memcpyFromArray(currentThread, J9_JNI_UNWRAP_REFERENCE(text), offset, text_length, textNativePtr);
            VM_ArrayCopyHelpers::memcpyFromArray(currentThread, J9_JNI_UNWRAP_REFERENCE(chaining_vector), offset, chaining_vector_length, chainingVectorNativePtr);
            VM_ArrayCopyHelpers::memcpyFromArray(currentThread, J9_JNI_UNWRAP_REFERENCE(hash), offset, hash_length, hashNativePtr);

            /* Do crypto operation */
            result = (*(cbcOp.SHA_op))(ruleArrayNativePtr,
                                       textNativePtr,
                                       chainingVectorNativePtr,
                                       hashNativePtr,
                                       return_code,
                                       reason_code,
                                       rule_array_count,
                                       text_length,
                                       rule_array_length);

            /* Negative ret means an error code, don't copy in that case */
            if (result >= 0) {
                /* Copy back from output native buffer to output arraylets */
                VM_ArrayCopyHelpers::memcpyToArray(currentThread, J9_JNI_UNWRAP_REFERENCE(hash), 0, hash_length, hashNativePtr);
            }

        } else {
            /* Throw OutOfMemory exception, unable to allocate buffer */
            result = -1;
            setNativeOutOfMemoryError(currentThread, J9NLS_VM_NATIVE_OOM);
        }

        j9mem_free_memory(ruleArrayNativePtr);
        j9mem_free_memory(textNativePtr);
        j9mem_free_memory(chainingVectorNativePtr);
        j9mem_free_memory(hashNativePtr);
    }

#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Exit digest\n");
#endif /* CRYPTO_DEBUG */

    return result;
}

jint JNICALL
Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KMAESE(J9VMThread *currentThread, jint icvMode, jint key_length, j9object_t key_identifier, jint initialization_vector_length, j9object_t initialization_vector, jint chain_data_length, j9object_t chain_data, jint clear_text_length, j9object_t clear_text, jint cipher_text_length, j9object_t cipher_text)
{
    TRACE_ENTER();
    TRACE2("clear_text_length=%d cipher_text_length=%d\n", clear_text_length, cipher_text_length);
    TRACE5("%p %p %p %p %p ---- \n", key_identifier, initialization_vector, chain_data, clear_text, cipher_text);

    cipher_request_t request;

    bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniHikmCryptoFunctions);
    if (!cryptoLibLoaded) {
        loadCryptoLib(currentThread);
    }
    cryptoPointers_t *cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniHikmCryptoFunctions;

    j9object_t keyIdentifierObj = J9_JNI_UNWRAP_REFERENCE(key_identifier);
    j9object_t initVectorObj    = J9_JNI_UNWRAP_REFERENCE(initialization_vector);
    j9object_t chainDataObj     = J9_JNI_UNWRAP_REFERENCE(chain_data);

    request.key_length                   = key_length;
    request.key_identifier               = (jbyte*)J9JAVAARRAY_EA(currentThread, keyIdentifierObj, 0, U_8);
    request.initialization_vector_length = initialization_vector_length;
    request.initialization_vector        = (jbyte*)J9JAVAARRAY_EA(currentThread, initVectorObj, 0, U_8);
    request.chain_data_length            = chain_data_length;
    request.chain_data                   = (jbyte*)J9JAVAARRAY_EA(currentThread, chainDataObj, 0, U_8);
    request.total_length                 = clear_text_length;      // total length before it is broken up into multiple calls
    request.input_length                 = clear_text_length;      // input_length
    request.output_length                = cipher_text_length;     // output_length
    request.icvMode                      = icvMode;

    jint result = aesCipher(currentThread,
                            clear_text,
                            cipher_text,
                            &request,
                            cryptFuncs->KMAESEPtr);
    TRACE_EXIT_RC(result);
    return result;
}

jint JNICALL
Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KMAESD(J9VMThread *currentThread, jint icvMode, jint key_length, j9object_t key_identifier, jint initialization_vector_length, j9object_t initialization_vector, jint chain_data_length, j9object_t chain_data, jint cipher_text_length, j9object_t cipher_text, jint clear_text_length, j9object_t clear_text)
{
    TRACE_ENTER();
    TRACE2("cipher_text_length=%d clear_text_length=%d\n", cipher_text_length, clear_text_length);
    TRACE5("%p %p %p %p %p ---- \n", key_identifier, initialization_vector, chain_data, clear_text, cipher_text);

    cipher_request_t request;

    bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniHikmCryptoFunctions);
    if (!cryptoLibLoaded) {
        loadCryptoLib(currentThread);
    }
    cryptoPointers_t *cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniHikmCryptoFunctions;

    j9object_t keyIdentifierObj = J9_JNI_UNWRAP_REFERENCE(key_identifier);
    j9object_t initVectorObj    = J9_JNI_UNWRAP_REFERENCE(initialization_vector);
    j9object_t chainDataObj     = J9_JNI_UNWRAP_REFERENCE(chain_data);

    request.key_length                   = key_length;
    request.key_identifier               = (jbyte*)J9JAVAARRAY_EA(currentThread, keyIdentifierObj, 0, U_8);
    request.initialization_vector_length = initialization_vector_length;
    request.initialization_vector        = (jbyte*)J9JAVAARRAY_EA(currentThread, initVectorObj, 0, U_8);
    request.chain_data_length            = chain_data_length;
    request.chain_data                   = (jbyte*)J9JAVAARRAY_EA(currentThread, chainDataObj, 0, U_8);
    request.total_length                 = cipher_text_length;      // total length before it is broken up into multiple calls
    request.input_length                 = cipher_text_length;      // input_length
    request.output_length                = clear_text_length;       // output_length
    request.icvMode                      = icvMode;

    jint result = aesCipher(currentThread,
                            cipher_text,
                            clear_text,
                            &request,
                            cryptFuncs->KMAESDPtr);
    TRACE_EXIT_RC(result);
    return result;
}

jint JNICALL
Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KMDESE(J9VMThread *currentThread, jint return_code, jint reason_code, jint rule_array_count, j9object_t rule_array, jint key_length, j9object_t key_identifier, jint initialization_vector_length, j9object_t initialization_vector, jint chain_data_length, j9object_t chain_data, jint clear_text_length, j9object_t clear_text, jint cipher_text_length, j9object_t cipher_text)
{
#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Enter Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KMDESE\n");
#endif /* CRYPTO_DEBUG */

    bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniHikmCryptoFunctions);
    if (!cryptoLibLoaded) {
        loadCryptoLib(currentThread);
    }
    cryptoPointers_t *cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniHikmCryptoFunctions;

    return cipher(currentThread,
                  return_code,
                  reason_code,
                  rule_array_count,
                  rule_array,
                  key_length,
                  key_identifier,
                  initialization_vector_length,
                  initialization_vector,
                  chain_data_length,
                  chain_data,
                  clear_text_length,      // input_length
                  clear_text,             // input
                  cipher_text_length,     // output_length
                  cipher_text,            // output
                  cryptFuncs->KMDESE,
                  cryptFuncs->KMDESEPtr,
                  0,
                  ENCRYPT,
                  DES);

}

jint JNICALL
Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KMDESD(J9VMThread *currentThread, jint return_code, jint reason_code, jint rule_array_count, j9object_t rule_array, jint key_length, j9object_t key_identifier, jint initialization_vector_length, j9object_t initialization_vector, jint chain_data_length, j9object_t chain_data, jint clear_text_length, j9object_t clear_text, jint cipher_text_length, j9object_t cipher_text)
{
#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Enter Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KMDESD\n");
#endif /* CRYPTO_DEBUG */

    bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniHikmCryptoFunctions);
    if (!cryptoLibLoaded) {
        loadCryptoLib(currentThread);
    }
    cryptoPointers_t *cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniHikmCryptoFunctions;

    return cipher(currentThread,
                  return_code,
                  reason_code,
                  rule_array_count,
                  rule_array,
                  key_length,
                  key_identifier,
                  initialization_vector_length,
                  initialization_vector,
                  chain_data_length,
                  chain_data,
                  clear_text_length,      // input_length
                  clear_text,             // input
                  cipher_text_length,     // output_length
                  cipher_text,            // output
                  cryptFuncs->KMDESD,
                  cryptFuncs->KMDESDPtr,
                  0,
                  DECRYPT,
                  DES);
}

jint JNICALL
Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KMGCME(J9VMThread *currentThread,
                                                 jint       key_length,
                                                 j9object_t key_identifier,
                                                 jint       initialization_vector_length,
                                                 j9object_t initialization_vector,
                                                 jint       clear_text_length,
                                                 j9object_t clear_text,
                                                 jint       AAD_data_length,                      /* AAD */
                                                 j9object_t AAD_data,
                                                 jint       cipher_text_length,
                                                 j9object_t cipher_text,
                                                 jint       authTagLen,
                                                 j9object_t authTag)
{
    TRACE_ENTER();
    TRACE4("initialization_vector_length=%d clear_text_length=%d AAD_data_length=%d cipher_text_length=%d\n",
           initialization_vector_length, clear_text_length, AAD_data_length, cipher_text_length);

    cipher_request_t request;

    if (NULL == currentThread->javaVM->jniHikmCryptoFunctions) {    // Has cryptoLib been loaded?
        loadCryptoLib(currentThread);
    }
    cryptoPointers_t *cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniHikmCryptoFunctions;

    j9object_t keyIdentifierObj = J9_JNI_UNWRAP_REFERENCE(key_identifier);
    j9object_t initVectorObj    = J9_JNI_UNWRAP_REFERENCE(initialization_vector);
    j9object_t aadObj           = J9_JNI_UNWRAP_REFERENCE(AAD_data);
    j9object_t authTagObj       = J9_JNI_UNWRAP_REFERENCE(authTag);

    request.key_length                   = key_length;
    request.key_identifier               = (jbyte*)J9JAVAARRAY_EA(currentThread, keyIdentifierObj, 0, U_8);
    request.initialization_vector_length = initialization_vector_length;
    request.initialization_vector        = (jbyte*)J9JAVAARRAY_EA(currentThread, initVectorObj, 0, U_8);
    request.total_length                 = clear_text_length;      // total length before it is broken up into multiple calls
    request.input_length                 = clear_text_length;      // input_length
    request.output_length                = cipher_text_length;     // output_length
    request.AAD_data_length              = AAD_data_length;
    request.AAD_data                     = (jbyte*)J9JAVAARRAY_EA(currentThread, aadObj, 0, U_8);
    request.authTagLen                   = authTagLen;
    request.AuthTag                      = (jbyte*)J9JAVAARRAY_EA(currentThread, authTagObj, 0, U_8);
    request.icvMode                      = ONLY;

    jint result = aesCipher(currentThread,
                            clear_text,
                            cipher_text,
                            &request,
                            cryptFuncs->KMGCMEPtr);
    TRACE_EXIT_RC(result);
    return result;
}

jint JNICALL
Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KMGCMD(J9VMThread *currentThread,
                                                 jint       key_length,
                                                 j9object_t key_identifier,
                                                 jint       initialization_vector_length,
                                                 j9object_t initialization_vector,
                                                 jint       cipher_text_length,
                                                 j9object_t cipher_text,
                                                 jint       AAD_data_length,                      /* AAD */
                                                 j9object_t AAD_data,
                                                 jint       clear_text_length,
                                                 j9object_t clear_text,
                                                 jint       authTagLen,
                                                 j9object_t authTag)
{
    TRACE_ENTER();
    TRACE4("initialization_vector_length=%d cipher_text_length=%d AAD_data_length=%d clear_text_length=%d\n",
           initialization_vector_length, cipher_text_length, AAD_data_length, clear_text_length);

    cipher_request_t request;

    if (NULL == currentThread->javaVM->jniHikmCryptoFunctions) {    // Has cryptoLib been loaded?
        loadCryptoLib(currentThread);
    }
    cryptoPointers_t *cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniHikmCryptoFunctions;

    j9object_t keyIdentifierObj = J9_JNI_UNWRAP_REFERENCE(key_identifier);
    j9object_t initVectorObj    = J9_JNI_UNWRAP_REFERENCE(initialization_vector);
    j9object_t aadObj           = J9_JNI_UNWRAP_REFERENCE(AAD_data);
    j9object_t authTagObj       = J9_JNI_UNWRAP_REFERENCE(authTag);

    request.key_length                   = key_length;
    request.key_identifier               = (jbyte*)J9JAVAARRAY_EA(currentThread, keyIdentifierObj, 0, U_8);
    request.initialization_vector_length = initialization_vector_length;
    request.initialization_vector        = (jbyte*)J9JAVAARRAY_EA(currentThread, initVectorObj, 0, U_8);
    request.total_length                 = cipher_text_length;      // total length before it is broken up into multiple calls
    request.input_length                 = cipher_text_length;      // input_length
    request.output_length                = clear_text_length;       // output_length
    request.AAD_data_length              = AAD_data_length;
    request.AAD_data                     = (jbyte*)J9JAVAARRAY_EA(currentThread, aadObj, 0, U_8);
    request.authTagLen                   = authTagLen;
    request.AuthTag                      = (jbyte*)J9JAVAARRAY_EA(currentThread, authTagObj, 0, U_8);
    request.icvMode                      = ONLY;

    jint result = aesCipher(currentThread,
                            cipher_text,
                            clear_text,
                            &request,
                            cryptFuncs->KMGCMDPtr);
    TRACE_EXIT_RC(result);
    return result;
}

jint JNICALL
Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA(J9VMThread *currentThread, jint return_code, jint reason_code, jint rule_array_count, j9object_t rule_array, jint rule_array_length, jint text_length, j9object_t text, jint chaining_vector_length, j9object_t chaining_vector, jint hash_length, j9object_t hash)
{
#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Enter Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA\n");
#endif /* CRYPTO_DEBUG */

    bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniHikmCryptoFunctions);
    if (!cryptoLibLoaded) {
        loadCryptoLib(currentThread);
    }
    cryptoPointers_t *cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniHikmCryptoFunctions;

    return digest(currentThread,
                  return_code,
                  reason_code,
                  rule_array_count,
                  rule_array,
                  rule_array_length,
                  text_length,
                  text,
                  chaining_vector_length,
                  chaining_vector,
                  hash_length,
                  hash,
                  cryptFuncs->KXMDSHA,
                  cryptFuncs->KXMDSHAPtr,
                  0,
                  0);
}

jint JNICALL
Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA224(J9VMThread *currentThread, jint return_code, jint reason_code, jint rule_array_count, j9object_t rule_array, jint rule_array_length, jint text_length, j9object_t text, jint chaining_vector_length, j9object_t chaining_vector, jint hash_length, j9object_t hash)
{
#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Enter Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA224\n");
#endif /* CRYPTO_DEBUG */

    bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniHikmCryptoFunctions);
    if (!cryptoLibLoaded) {
        loadCryptoLib(currentThread);
    }
    cryptoPointers_t *cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniHikmCryptoFunctions;

    return digest(currentThread,
                  return_code,
                  reason_code,
                  rule_array_count,
                  rule_array,
                  rule_array_length,
                  text_length,
                  text,
                  chaining_vector_length,
                  chaining_vector,
                  hash_length,
                  hash,
                  cryptFuncs->KXMDSHA224,
                  cryptFuncs->KXMDSHA224Ptr,
                  0,
                  0);
}

jint JNICALL
Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA256(J9VMThread *currentThread, jint return_code, jint reason_code, jint rule_array_count, j9object_t rule_array, jint rule_array_length, jint text_length, j9object_t text, jint chaining_vector_length, j9object_t chaining_vector, jint hash_length, j9object_t hash)
{
#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Enter Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA256\n");
#endif /* CRYPTO_DEBUG */

    bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniHikmCryptoFunctions);
    if (!cryptoLibLoaded) {
        loadCryptoLib(currentThread);
    }
    cryptoPointers_t *cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniHikmCryptoFunctions;

    return digest(currentThread,
                  return_code,
                  reason_code,
                  rule_array_count,
                  rule_array,
                  rule_array_length,
                  text_length,
                  text,
                  chaining_vector_length,
                  chaining_vector,
                  hash_length,
                  hash,
                  cryptFuncs->KXMDSHA256,
                  cryptFuncs->KXMDSHA256Ptr,
                  0,
                  0);
}

jint JNICALL
Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA384(J9VMThread *currentThread, jint return_code, jint reason_code, jint rule_array_count, j9object_t rule_array, jint rule_array_length, jint text_length, j9object_t text, jint chaining_vector_length, j9object_t chaining_vector, jint hash_length, j9object_t hash)
{
#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Enter Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA384\n");
#endif /* CRYPTO_DEBUG */

    bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniHikmCryptoFunctions);
    if (!cryptoLibLoaded) {
        loadCryptoLib(currentThread);
    }
    cryptoPointers_t *cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniHikmCryptoFunctions;

    return digest(currentThread,
                  return_code,
                  reason_code,
                  rule_array_count,
                  rule_array,
                  rule_array_length,
                  text_length,
                  text,
                  chaining_vector_length,
                  chaining_vector,
                  hash_length,
                  hash,
                  cryptFuncs->KXMDSHA384,
                  cryptFuncs->KXMDSHA384Ptr,
                  0,
                  0);
}

jint JNICALL
Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA512(J9VMThread *currentThread, jint return_code, jint reason_code, jint rule_array_count, j9object_t rule_array, jint rule_array_length, jint text_length, j9object_t text, jint chaining_vector_length, j9object_t chaining_vector, jint hash_length, j9object_t hash)
{
#if defined(CRYPTO_DEBUG)
    fprintf(stderr,"Enter Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA512\n");
#endif /* CRYPTO_DEBUG */

    bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniHikmCryptoFunctions);
    if (!cryptoLibLoaded) {
        loadCryptoLib(currentThread);
    }
    cryptoPointers_t *cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniHikmCryptoFunctions;

    return digest(currentThread,
                  return_code,
                  reason_code,
                  rule_array_count,
                  rule_array,
                  rule_array_length,
                  text_length,
                  text,
                  chaining_vector_length,
                  chaining_vector,
                  hash_length,
                  hash,
                  cryptFuncs->KXMDSHA512,
                  cryptFuncs->KXMDSHA512Ptr,
                  0,
                  0);
}

J9_FAST_JNI_METHOD_TABLE(com_ibm_crypto_hdwrCCA_provider_HIKM)
    J9_FAST_JNI_METHOD("KMAESE", "(II[BI[BI[BI[BI[B)I", Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KMAESE,
        J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
    J9_FAST_JNI_METHOD("KMAESD", "(II[BI[BI[BI[BI[B)I", Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KMAESD,
        J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
    J9_FAST_JNI_METHOD("KMDESE", "(III[BI[BI[BI[BI[BI[B)I", Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KMDESE,
        J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
    J9_FAST_JNI_METHOD("KMDESD", "(III[BI[BI[BI[BI[BI[B)I", Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KMDESD,
        J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
    J9_FAST_JNI_METHOD("KMGCME", "(I[BI[BI[BI[BI[BI[B)I", Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KMGCME,
        J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
    J9_FAST_JNI_METHOD("KMGCMD", "(I[BI[BI[BI[BI[BI[B)I", Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KMGCMD,
        J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
    J9_FAST_JNI_METHOD("KXMDSHA", "(III[BII[BI[BI[B)I", Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA,
        J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
    J9_FAST_JNI_METHOD("KXMDSHA224", "(III[BII[BI[BI[B)I", Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA224,
        J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
    J9_FAST_JNI_METHOD("KXMDSHA256", "(III[BII[BI[BI[B)I", Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA256,
        J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
    J9_FAST_JNI_METHOD("KXMDSHA384", "(III[BII[BI[BI[B)I", Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA384,
        J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
    J9_FAST_JNI_METHOD("KXMDSHA512", "(III[BII[BI[BI[B)I", Fast_com_ibm_crypto_hdwrCCA_provider_HIKM_KXMDSHA512,
        J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
J9_FAST_JNI_METHOD_TABLE_END

}
