/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

extern "C" {

typedef int CIPHER_t(long iccContextId, long iccCipherId, void* input, int inputLen, void* output, int needsReinit);
typedef int z_kmc_native_t(void* input, void* output, int inputLength, long paramPointer, int mode);

typedef union {
	CIPHER_t* CIPHER_op;
	z_kmc_native_t* z_kmc_native;
} cryptoFunc_t;

typedef struct cryptoPointers {
	cryptoFunc_t CIPHER_encryptFinalPtr;
	cryptoFunc_t CIPHER_decryptFinalPtr;
	cryptoFunc_t CIPHER_encryptUpdatePtr;
	cryptoFunc_t CIPHER_decryptUpdatePtr;
	cryptoFunc_t z_kmc_nativePtr;
	CIPHER_t* CIPHER_encryptUpdate;
	CIPHER_t* CIPHER_decryptUpdate;
} cryptoPointers_t;


/* loads crypto lib routines and store routine pointers for use.
 * Need to synchronize this operation so that threads won't
 * use the routines too early
 * */
void
loadCryptoLib(J9VMThread *currentThread)
{
	if (NULL == currentThread->javaVM->jniCryptoFunctions) {
		omrthread_monitor_t mutex = currentThread->javaVM->jniCryptoLibLock;
		omrthread_monitor_enter(mutex);
		if (NULL == currentThread->javaVM->jniCryptoFunctions) {
			PORT_ACCESS_FROM_VMC(currentThread);
			cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)j9mem_allocate_memory(sizeof(cryptoPointers_t), J9MEM_CATEGORY_VM_JCL);
			UDATA* cryptoLibrary = &(currentThread->javaVM->jniCryptoLibrary);
			CIPHER_t* CIPHER_encryptFinal = NULL;
			CIPHER_t* CIPHER_decryptFinal = NULL;
			z_kmc_native_t* z_kmc_native = NULL;
			const char * cryptolib_name = "jgskit";
			const char * encryptFinal_name = "CIPHER_encryptFinal_internal";
			const char * decryptFinal_name = "CIPHER_decryptFinal_internal";
			const char * encryptUpdate_name = "CIPHER_encryptUpdate_internal";
			const char * decryptUpdate_name = "CIPHER_decryptUpdate_internal";
			const char * zKMC_name = "CIPHER_zKMC_internal";

			if (NULL == cryptFuncs) goto oomFailed;
			if (j9sl_open_shared_library((char*)cryptolib_name, cryptoLibrary, J9PORT_SLOPEN_DECORATE)) goto loadFailed;
			if (j9sl_lookup_name(*cryptoLibrary, (char*)encryptFinal_name, (UDATA*)&CIPHER_encryptFinal, NULL)) goto loadFailed;
			if (j9sl_lookup_name(*cryptoLibrary, (char*)decryptFinal_name, (UDATA*)&CIPHER_decryptFinal, NULL)) goto loadFailed;
			if (j9sl_lookup_name(*cryptoLibrary, (char*)encryptUpdate_name, (UDATA*)&(cryptFuncs->CIPHER_encryptUpdate), NULL)) goto loadFailed;
			if (j9sl_lookup_name(*cryptoLibrary, (char*)decryptUpdate_name, (UDATA*)&(cryptFuncs->CIPHER_decryptUpdate), NULL)) goto loadFailed;
			if (j9sl_lookup_name(*cryptoLibrary, (char*)zKMC_name, (UDATA*)&z_kmc_native, NULL)) goto loadFailed;
#if defined(CRYPTO_DEBUG)
			printf("** Loading cryptography library jgskit\n");
			printf("** CIPHER_encryptFinal: %p\n",CIPHER_encryptFinal);
			printf("** CIPHER_decryptFinal: %p\n",CIPHER_decryptFinal);
			printf("** CIPHER_encryptUpdate: %p\n",cryptFuncs->CIPHER_encryptUpdate);
			printf("** CIPHER_decryptUpdate: %p\n",cryptFuncs->CIPHER_decryptUpdate);
			printf("** z_kmc_native: %p\n",z_kmc_native);
#endif /* CRYPTO_DEBUG */
			cryptFuncs->CIPHER_encryptFinalPtr.CIPHER_op = CIPHER_encryptFinal;
			cryptFuncs->CIPHER_encryptUpdatePtr.CIPHER_op = cryptFuncs->CIPHER_encryptUpdate;
			cryptFuncs->CIPHER_decryptFinalPtr.CIPHER_op = CIPHER_decryptFinal;
			cryptFuncs->CIPHER_decryptUpdatePtr.CIPHER_op = cryptFuncs->CIPHER_decryptUpdate;
			cryptFuncs->z_kmc_nativePtr.z_kmc_native = z_kmc_native;
			currentThread->javaVM->jniCryptoFunctions = (void*)cryptFuncs;
			goto exit;
		}
oomFailed:
		setNativeOutOfMemoryError(currentThread, J9NLS_VM_NATIVE_OOM);
		goto exit;
loadFailed:
		/* vm assert if the library does not load correctly
		 * the crypto library is part of the jdk and a incorrect loading
		 * indicates a corruption or a bug in the jdk */ 	
		Assert_VM_internalError();
exit:
		omrthread_monitor_exit(mutex);
	}
}

static VMINLINE bool
isContiguousRange2(J9VMThread *currentThread, UDATA index, UDATA count)
{
	UDATA arrayletLeafSizeInElements = currentThread->javaVM->arrayletLeafSize;
	UDATA lastIndex = index;

	lastIndex += count;
	lastIndex -= 1;
	lastIndex ^= index;
	return lastIndex < arrayletLeafSizeInElements;
}

/* This routine handles encrypt and decrypt operations for AES operations.
 * This routines calls the passed function pointer on the input data, this function may encrypt or decrypt.
 * The input and output are always byte arrays.
 *
 * If the input data is very large, the  VMAccess lock may be acquired for too extended a duration. In that case,
 * VMAccess is released and re-acquired after a fixed number of bytes have been processed to allow the GC to operate.
 *
 * The maxinum number of bytes to be processed at a single operation was selected as follows: VMAccess should not be acquired for no more than 0.1ms.
 * Benchmarks indicate current performance (2020) is at 620 MB/s for AES-CBC encrypt at 16 threads on x86 linux.
 * Other scenarios & platforms are all faster. Using 620 MB/s as worst case performance, we have 62KB processed at 0.1ms
 *
 * there are no error checking implemented here on input and output buffers, all errors are handled by JCL code.
 */

#define OP_SIZE_LIMIT 63488

VMINLINE jint
AESLaunch(J9VMThread *currentThread, jlong iccContextId, jlong iccCipherId, j9object_t input, jint inputOffset,jint inputLen, j9object_t output, jint outputOffset, jboolean needsReinit, CIPHER_t* cbcOpUpdate, cryptoFunc_t cbcOp, jlong paramPointer, jint mode)
{
	j9object_t in = input;
	j9object_t out = output;
	int inputIndex = inputOffset;
	int outputIndex = outputOffset;
	void* inputAddress = NULL;
	void* outputAddress = J9JAVAARRAY_EA(currentThread, out, outputIndex, U_8);
	jint totalBytes = inputLen;
	jint result = 0;

	if (NULL != in) {
		inputAddress = J9JAVAARRAY_EA(currentThread, in, inputIndex, U_8);
	}
#if defined(CRYPTO_DEBUG)
	printf("** Enter AESLaunch %p + %d (%d) -> %p + %d (%lld %lld)\n", inputAddress, inputIndex, inputLen, outputAddress, outputIndex, (long long int)iccContextId, (long long int)iccCipherId);
	if (totalBytes > OP_SIZE_LIMIT) {
		printf("** Size limit of %d reached, %d bytes to process\n", OP_SIZE_LIMIT, totalBytes);
	}
	if (0 != paramPointer) {
		printf("** Launch Single Shot z_kmc_native, calling %p\n",cbcOp.z_kmc_native);
	} else {
		printf("** Launch Single Shot cbcOp.CIPHER_op, calling %p\n",cbcOp.CIPHER_op);
	}
#endif /* CRYPTO_DEBUG */
	if (totalBytes <= OP_SIZE_LIMIT) {
		if (0 != paramPointer) {
			result = (*(cbcOp.z_kmc_native))(inputAddress, outputAddress, totalBytes, (long)paramPointer, mode);
		} else {
			result = (*(cbcOp.CIPHER_op))((long)iccContextId, (long)iccCipherId, inputAddress, totalBytes, outputAddress, needsReinit);
		}
	} else {
		/* the requested operation is too large to be done in a single op since that would acquire VMAccess for too long
		 * so the operation is broken up */
		int returnCode = 0;
		jint currentBytes = 0;
		jint bytesToOp = totalBytes;

		while (0 != bytesToOp) {
#if defined(CRYPTO_DEBUG)
			printf("** %d bytes out of %d\n", bytesToOp, totalBytes);
#endif /* CRYPTO_DEBUG */
			currentBytes = ((bytesToOp - OP_SIZE_LIMIT) >= 0) ? OP_SIZE_LIMIT : bytesToOp;
			if (0 != paramPointer) {
				returnCode = (*(cbcOp.z_kmc_native))(inputAddress, outputAddress, currentBytes, (long)paramPointer, mode);
			} else {
				if (bytesToOp == totalBytes) {
#if defined(CRYPTO_DEBUG)
					printf("** Launch first cbcOp.CIPHER_op with %d bytes, (%p -> %p)\n",(int)currentBytes,inputAddress,outputAddress);
#endif /* CRYPTO_DEBUG */
					/* first call, pass the needsReinit flag and call update */
					returnCode = (*cbcOpUpdate)((long)iccContextId, (long)iccCipherId, inputAddress, currentBytes, outputAddress, needsReinit);
				} else if ((bytesToOp - currentBytes) > 0) {
#if defined(CRYPTO_DEBUG)
					printf("** Launch subseq but not final cbcOp.CIPHER_op with %d bytes, (%p -> %p)\n",(int)currentBytes,inputAddress,outputAddress);
#endif /* CRYPTO_DEBUG */
					/* subsequent but not final call, call update and do not pass needsReinit flag */
					returnCode = (*cbcOpUpdate)((long)iccContextId, (long)iccCipherId, inputAddress, currentBytes, outputAddress, JNI_FALSE);
				} else {
#if defined(CRYPTO_DEBUG)
					printf("** Launch final cbcOp.CIPHER_op with %d bytes, (%p -> %p)\n",(int)currentBytes,inputAddress,outputAddress);
#endif /* CRYPTO_DEBUG */
					/* final call, call final and do not pass needsReinit flag */
					returnCode = (*(cbcOp.CIPHER_op))((long)iccContextId, (long)iccCipherId, inputAddress, currentBytes, outputAddress, JNI_FALSE);
					/* operation done, return */
					result = returnCode < 0 ? returnCode : (result + returnCode);
					break;
				}
			}
			if (returnCode < 0) {
				/* Crypto returned error, returning immediately */
				result = returnCode;
				break;
			}
			/* no error, crypto returns length */
			result += returnCode;
			/*
			 * release VMAccess to allow GC to work if needed
			 * recover input/output pointers afterwards
			 */
			if (J9_ARE_ANY_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_ANY)) {
				J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;
				
				PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, in);
				PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, out);
				vmFuncs->internalReleaseVMAccess(currentThread);
				vmFuncs->internalAcquireVMAccess(currentThread);
				out = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
				in = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
			}
			inputIndex += currentBytes;
			outputIndex += returnCode;
			inputAddress = J9JAVAARRAY_EA(currentThread, in, inputIndex, U_8);
			outputAddress = J9JAVAARRAY_EA(currentThread, out, outputIndex, U_8);
#if defined(CRYPTO_DEBUG)
			printf("** Recompute Addresses : %p %p %d %d\n",inputAddress, outputAddress, inputIndex, outputIndex);
#endif /* CRYPTO_DEBUG */
			bytesToOp -= currentBytes;
		}
	}
	return result;
}

/*
 * AESLaunch_discontiguous handles AES operations when the input and/or output arrays are in arraylet form and therefore discontiguous.
 * Temporary buffers are used for input and output for the AES operations since AES operations do not support fragmented output arrays.
 */
VMINLINE jint
AESLaunch_discontiguous(J9VMThread *currentThread, jlong iccContextId, jlong iccCipherId, j9object_t input, jint inputOffset,jint inputLen, j9object_t output, jint outputOffset, jboolean needsReinit, CIPHER_t* cbcOpUpdate, cryptoFunc_t cbcOp, jlong paramPointer, jint mode)
{
	jint ret = 0;
	I_32 outputArraySize = J9INDEXABLEOBJECT_SIZE(currentThread, output);
	PORT_ACCESS_FROM_VMC(currentThread);

	/* allocate native buffers to be used to directly interact with crypto API */
	void* inputNativePtr = j9mem_allocate_memory(inputLen, J9MEM_CATEGORY_VM_JCL);
	void* outputNativePtr = j9mem_allocate_memory(outputArraySize, J9MEM_CATEGORY_VM_JCL);

	if (NULL != inputNativePtr && NULL != outputNativePtr) {
		/* copy from input arraylets to native buffer */
		VM_ArrayCopyHelpers::memcpyFromArray(currentThread, input, inputOffset, inputLen, inputNativePtr);
		/* do crypto operation */
		if (0 != paramPointer) {
#if defined(CRYPTO_DEBUG)
			printf("** Launch Single Shot z_kmc_native with %d bytes, calling %p from %p to %p\n",(int)inputLen,cbcOp.z_kmc_native,inputNativePtr,outputNativePtr);
#endif /* CRYPTO_DEBUG */
			ret = (*(cbcOp.z_kmc_native))(inputNativePtr, outputNativePtr, inputLen, (long)paramPointer, mode);
		} else {
#if defined(CRYPTO_DEBUG)
			printf("** Launch Single Shot cbcOp.CIPHER_op with %d bytes, calling %p from %p to %p\n",(int)inputLen,cbcOp.CIPHER_op,inputNativePtr,outputNativePtr);
#endif /* CRYPTO_DEBUG */
			ret = (*(cbcOp.CIPHER_op))((long)iccContextId, (long)iccCipherId, inputNativePtr, inputLen, outputNativePtr, needsReinit);
		}
		/* copy back from output native buffer to output arraylets */
#if defined(CRYPTO_DEBUG)
		printf("*** Done. Copy back and free %d\n", ret);
#endif /* CRYPTO_DEBUG */
		/* negative ret means an error code, don't copy in that case */
		if (ret > 0) {
			VM_ArrayCopyHelpers::memcpyToArray(currentThread, output, outputOffset, ret, outputNativePtr);
		}
		/* release input and output native buffers */
		j9mem_free_memory(outputNativePtr);
		j9mem_free_memory(inputNativePtr);
	} else {
		/* throw OOM exception, unable to allocate buffer */
		ret = -1;
		setNativeOutOfMemoryError(currentThread, J9NLS_VM_NATIVE_OOM);
	}
	/* return output size or error code */
	return ret;
}
/*
 * Main entry point into AES encrypt/decrypt routine, here it is checked if the input and output arrays are discontigious, if they are
 * the crypto routine is called on each arraylet.
 *
 * It further calls AESLaunch which handles the actual invokation and also handles pauses for very large arrays.
 * The basic implementation was copied from the fastjni arraycopy implementation
 */
VMINLINE jint
AES(J9VMThread *currentThread, jlong iccContextId, jlong iccCipherId, j9object_t input, jint inputOffset,jint inputLen, j9object_t output, jint outputOffset, jboolean needsReinit, CIPHER_t* cbcOpUpdate, cryptoFunc_t cbcOp, jlong paramPointer, jint mode)
{
	I_32 outputArraySize = J9INDEXABLEOBJECT_SIZE(currentThread, output);
	bool ctgr2_input = isContiguousRange2(currentThread, inputOffset, inputLen);
	bool ctgr2_output = isContiguousRange2(currentThread, outputOffset,outputArraySize);
	jint result;
	bool discont_test = (((inputLen > 0 && outputArraySize > 0) && (ctgr2_input && ctgr2_output))
				|| ((inputLen == 0 && outputArraySize > 0) && ctgr2_output)
				|| ((inputLen > 0 && outputArraySize == 0) && ctgr2_input)
				|| ((inputLen == 0 && outputArraySize == 0)));

#if defined(CRYPTO_DEBUG)
	if (discont_test) {
		printf("** AESLaunch: %d %d %d %d %p\n", inputOffset, inputLen, outputOffset, needsReinit, cbcOpUpdate);
	} else {
		printf("** AESLaunch discontiguous: %d %d %d %d %p\n", inputOffset, inputLen, outputOffset, needsReinit, cbcOpUpdate);
	}
#endif /* CRYPTO_DEBUG */
	if (discont_test) {
		result = AESLaunch(currentThread, iccContextId, iccCipherId, input, inputOffset, inputLen, output, outputOffset, needsReinit, cbcOpUpdate, cbcOp, paramPointer, mode);
	} else {
		result = AESLaunch_discontiguous(currentThread, iccContextId, iccCipherId, input, inputOffset, inputLen, output, outputOffset, needsReinit, cbcOpUpdate, cbcOp, paramPointer, mode);
	}
	return result;
}

jint JNICALL
Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_encryptFinal(J9VMThread *currentThread, jlong iccContextId, jlong iccCipherId, j9object_t plaintext, jint plaintextOffset,jint plaintextLen, j9object_t ciphertext, jint ciphertextOffset, jboolean needsReinit)
{
	bool cryptoLibLoaded = !(NULL == currentThread->javaVM->jniCryptoFunctions);
	if (!cryptoLibLoaded) {
		loadCryptoLib(currentThread);
	}
	cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniCryptoFunctions;
	return AES(currentThread, iccContextId, iccCipherId, plaintext, plaintextOffset, plaintextLen, ciphertext, ciphertextOffset, needsReinit, cryptFuncs->CIPHER_encryptUpdate, cryptFuncs->CIPHER_encryptFinalPtr, 0, 0);
}


jint JNICALL
Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_decryptFinal(J9VMThread *currentThread, jlong iccContextId, jlong iccCipherId, j9object_t ciphertext, jint ciphertextOffset,jint ciphertextLen, j9object_t plaintext, jint plaintextOffset, jboolean needsReinit)
{
	bool cryptoLibLoaded = !(NULL == currentThread->javaVM->jniCryptoFunctions);
	if (!cryptoLibLoaded) {
		loadCryptoLib(currentThread);
	}
	cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniCryptoFunctions;
	return AES(currentThread, iccContextId, iccCipherId, ciphertext, ciphertextOffset, ciphertextLen, plaintext, plaintextOffset, needsReinit, cryptFuncs->CIPHER_decryptUpdate, cryptFuncs->CIPHER_decryptFinalPtr, 0, 0);
}


jint JNICALL
Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_encryptUpdate(J9VMThread *currentThread, jlong iccContextId, jlong iccCipherId, j9object_t plaintext, jint plaintextOffset,jint plaintextLen, j9object_t ciphertext, jint ciphertextOffset, jboolean needsReinit)
{
	bool cryptoLibLoaded = !(NULL == currentThread->javaVM->jniCryptoFunctions);
	if (!cryptoLibLoaded) {
		loadCryptoLib(currentThread);
	}
	cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniCryptoFunctions;
	return AES(currentThread, iccContextId, iccCipherId, plaintext, plaintextOffset, plaintextLen, ciphertext, ciphertextOffset, needsReinit, cryptFuncs->CIPHER_encryptUpdate, cryptFuncs->CIPHER_encryptUpdatePtr, 0, 0);
}

jint JNICALL
Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_decryptUpdate(J9VMThread *currentThread, jlong iccContextId, jlong iccCipherId, j9object_t ciphertext, jint ciphertextOffset,jint ciphertextLen, j9object_t plaintext, jint plaintextOffset, jboolean needsReinit)
{
	bool cryptoLibLoaded = !(NULL == currentThread->javaVM->jniCryptoFunctions);
	if (!cryptoLibLoaded) {
		loadCryptoLib(currentThread);
	}
	cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniCryptoFunctions;
	return AES(currentThread, iccContextId, iccCipherId, ciphertext, ciphertextOffset, ciphertextLen, plaintext, plaintextOffset, needsReinit, cryptFuncs->CIPHER_decryptUpdate, cryptFuncs->CIPHER_decryptUpdatePtr, 0, 0);
}

jint JNICALL
Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_z_kmc_native(J9VMThread *currentThread, j9object_t input, jint inputOffset, j9object_t output, jint outputOffset, jlong paramPointer, jint inputLength, jint mode)
{
	bool cryptoLibLoaded = !(NULL == currentThread->javaVM->jniCryptoFunctions);
	if (!cryptoLibLoaded) {
		loadCryptoLib(currentThread);
	}
	cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniCryptoFunctions;
	return AES(currentThread, 0L, 0L, input, inputOffset, inputLength, output, outputOffset, JNI_FALSE, NULL, cryptFuncs->z_kmc_nativePtr, paramPointer, mode);
}

J9_FAST_JNI_METHOD_TABLE(com_ibm_crypto_plus_provider_icc_NativeInterface)
	J9_FAST_JNI_METHOD("CIPHER_decryptUpdate", "(JJ[BII[BIZ)I", Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_decryptUpdate,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("CIPHER_encryptUpdate", "(JJ[BII[BIZ)I", Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_encryptUpdate,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("CIPHER_encryptFinal", "(JJ[BII[BIZ)I", Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_encryptFinal,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("CIPHER_decryptFinal", "(JJ[BII[BIZ)I", Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_decryptFinal,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("z_kmc_native", "([BI[BIJII)I", Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_z_kmc_native,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
J9_FAST_JNI_METHOD_TABLE_END
}
