/******************************************************************************
 * Licensed Materials - Property of IBM
 * "Restricted Materials of IBM"
 *
 * (c) Copyright IBM Corp. 2020, 2023 All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or disclosure
 * restricted by GSA ADP Schedule Contract with IBM Corp.
 ******************************************************************************/

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

#define J9_CRYPTO_DLL_NAME (char*)"jgskit"

#if !defined(PATH_MAX)
/* This is a somewhat arbitrarily selected fixed buffer size. */
#define PATH_MAX 1024
#endif

/* controls the maximum number of bytes that can be processed at a time before VMAccess is released by the crypto AES algorithm */
#define AES_OP_SIZE_LIMIT 63488
#define HMAC_OP_SIZE_LIMIT 16384 
#define DIGEST_OP_SIZE_LIMIT 1024

extern "C" {

typedef int CIPHER_t(void* iccContextId, void* iccCipherId, void* input, int inputLen, void* output, int needsReinit);
typedef int z_kmc_native_t(void* input, void* output, int inputLength, long paramPointer, int mode);
typedef int HMAC_update_t(void* iccCtx, void* iccHMAC, void* key, int keySize, void* input, int inputLen, int needInit);
typedef int HMAC_doFinal_t(void* iccCtx, void* iccHMAC, void* key, int keySize, void* hmac, int needInit);
typedef int DIGEST_update_t(void* iccCtx, void* iccDIGEST, void* input, int inputLen);
typedef int DIGEST_digest_t(void* iccCtx, void* iccDIGEST, void* digest);

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
	HMAC_update_t* HMAC_update;
	HMAC_doFinal_t* HMAC_doFinal;
	DIGEST_update_t* DIGEST_update;
	DIGEST_digest_t* DIGEST_digest;
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
	omrthread_monitor_t mutex = currentThread->javaVM->jniCryptoLibLock;

	omrthread_monitor_enter(mutex);
	if (NULL == currentThread->javaVM->jniCryptoFunctions) {
		PORT_ACCESS_FROM_VMC(currentThread);
		cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)j9mem_allocate_memory(sizeof(cryptoPointers_t), J9MEM_CATEGORY_VM_JCL);
		UDATA* cryptoLibrary = &(currentThread->javaVM->jniCryptoLibrary);
		CIPHER_t* CIPHER_encryptFinal = NULL;
		CIPHER_t* CIPHER_decryptFinal = NULL;
		z_kmc_native_t* z_kmc_native = NULL;
		J9PortLibrary * portLib = currentThread->javaVM->portLibrary;
		J9JavaVM* vm = currentThread->javaVM;
		BOOLEAN fips140_2 = vm->fipsHome == vm->javaHome;
		char* dir = fips140_2 ? vm->j2seRootDirectory : (char *)vm->fipsHome;
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
				if (fips140_2) {
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
					}
				} else {
					/* expectedPathLength - %s/lib/icc/%s - +7 includes "lib", "icc" and NUL terminator */
					UDATA expectedPathLength = strlen(dir) + strlen(J9_CRYPTO_DLL_NAME) + (strlen(DIR_SEPARATOR_STR) * 3) + 7;
					if (expectedPathLength > PATH_MAX) {
						correctPathPtr = (char*)j9mem_allocate_memory(expectedPathLength, J9MEM_CATEGORY_VM_JCL);
						if (NULL == correctPathPtr) {
							setNativeOutOfMemoryError(currentThread, J9NLS_VM_NATIVE_OOM);
						}
					}
					if (NULL != correctPathPtr) {
						j9str_printf(portLib, correctPathPtr, expectedPathLength, "%s%slib%sicc%s%s", dir, DIR_SEPARATOR_STR, DIR_SEPARATOR_STR, DIR_SEPARATOR_STR, J9_CRYPTO_DLL_NAME);
					}
				}
				if (NULL != correctPathPtr) {
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

			if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"CIPHER_encryptFinal_internal", (UDATA*)&CIPHER_encryptFinal, "iJJLiLZ")) {
				Assert_VM_internalError();
			}
			if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"CIPHER_decryptFinal_internal", (UDATA*)&CIPHER_decryptFinal, "iJJLiLZ")) {
				Assert_VM_internalError();
			}
			if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"CIPHER_encryptUpdate_internal", (UDATA*)&(cryptFuncs->CIPHER_encryptUpdate), "iJJLiLZ")) {
				Assert_VM_internalError();
			}
			if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"CIPHER_decryptUpdate_internal", (UDATA*)&(cryptFuncs->CIPHER_decryptUpdate), "iJJLiLZ")) {
				Assert_VM_internalError();
			}
			if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"CIPHER_zKMC_internal", (UDATA*)&z_kmc_native, "iLLiji")) {
				Assert_VM_internalError();
			}
			if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"HMAC_update_internal", (UDATA*)&(cryptFuncs->HMAC_update), "iJJLiLZ")) {
				Assert_VM_internalError();
			}
			if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"HMAC_doFinal_internal", (UDATA*)&(cryptFuncs->HMAC_doFinal), "iJJLiZL")) {
				Assert_VM_internalError();
			}
			if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"DIGEST_update_internal", (UDATA*)&(cryptFuncs->DIGEST_update), "iJJi")) {
				Assert_VM_internalError();
			}
			if (0 != j9sl_lookup_name(*cryptoLibrary, (char*)"DIGEST_digest_and_reset_internal", (UDATA*)&(cryptFuncs->DIGEST_digest), "iJJ")) {
				Assert_VM_internalError();
			}
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"** Loading cryptography library jgskit\n");
			fprintf(stderr,"** CIPHER_encryptFinal: %p\n", CIPHER_encryptFinal);
			fprintf(stderr,"** CIPHER_decryptFinal: %p\n", CIPHER_decryptFinal);
			fprintf(stderr,"** CIPHER_encryptUpdate: %p\n", cryptFuncs->CIPHER_encryptUpdate);
			fprintf(stderr,"** CIPHER_decryptUpdate: %p\n", cryptFuncs->CIPHER_decryptUpdate);
			fprintf(stderr,"** z_kmc_native: %p\n", z_kmc_native);
			fprintf(stderr,"** HMAC_update: %p\n", cryptFuncs->HMAC_update);
			fprintf(stderr,"** HMAC_doFinal: %p\n", cryptFuncs->HMAC_doFinal);
			fprintf(stderr,"** DIGEST_update: %p\n", cryptFuncs->DIGEST_update);
			fprintf(stderr,"** DIGEST_digest: %p\n", cryptFuncs->DIGEST_digest);
#endif /* CRYPTO_DEBUG */
			cryptFuncs->CIPHER_encryptFinalPtr.CIPHER_op = CIPHER_encryptFinal;
			cryptFuncs->CIPHER_encryptUpdatePtr.CIPHER_op = cryptFuncs->CIPHER_encryptUpdate;
			cryptFuncs->CIPHER_decryptFinalPtr.CIPHER_op = CIPHER_decryptFinal;
			cryptFuncs->CIPHER_decryptUpdatePtr.CIPHER_op = cryptFuncs->CIPHER_decryptUpdate;
			cryptFuncs->z_kmc_nativePtr.z_kmc_native = z_kmc_native;
			vm->jniCryptoFunctions = (void*)cryptFuncs;
			if ((correctPath != correctPathPtr) && (NULL != correctPathPtr)) {
				j9mem_free_memory(correctPathPtr);
			}
		}
	}
	omrthread_monitor_exit(mutex);
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


/* 
 * This routine handles encrypt and decrypt operations for AES operations.
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
 *
 * @param[in] currentThread - current thread
 * @param[in] iccContextId - native library context
 * @param[in] iccCipherId - cipher state context
 * @param[in] input - input byte array
 * @param[in] inputOffset - starting offset for input
 * @param[in] inputLen - length of input data
 * @param[in] output - output byte array
 * @param[in] outputOffset - starting offset for output
 * @param[in] needsReinit - flag indicating if operation requires a reinit
 * @param[in] cbcOpUpdate - function pointer for AES Update needed when a large input has to be broken up to segments 
 * @param[in] cbcOp - function pointer for requested AES operation 
 * @param[in] paramPointer - pointer to internal state used by z14 and higher Z-CPUs
 * @param[in] mode - mode used by z14 and higher Z-CPUs
 *
 * @return - if the value is negative, this is a returned error code. otherwise the value is the return length
 */
static VMINLINE jint
AESLaunch(J9VMThread *currentThread, jlong iccContextId, jlong iccCipherId, jobject input, jint inputOffset, jint inputLen, jobject output, jint outputOffset, jboolean needsReinit, CIPHER_t* cbcOpUpdate, cryptoFunc_t cbcOp, jlong paramPointer, jint mode)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** Enter AESLaunch %p %p --- ", input, output);
#endif /* CRYPTO_DEBUG */
	j9object_t in = NULL;
	j9object_t out = J9_JNI_UNWRAP_REFERENCE(output);
	int inputIndex = inputOffset;
	int outputIndex = outputOffset;
	void* inputAddress = NULL;
	void* outputAddress = J9JAVAARRAY_EA(currentThread, out, outputIndex, U_8);
	jint totalBytes = inputLen;
	jint result = 0;

	if (NULL != input) {
		in = J9_JNI_UNWRAP_REFERENCE(input);
		inputAddress = J9JAVAARRAY_EA(currentThread, in, inputIndex, U_8);
	}
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"%p + %d (%d) -> %p + %d (%lld %lld)\n", inputAddress, inputIndex, inputLen, outputAddress, outputIndex, (long long int)iccContextId, (long long int)iccCipherId);
	if (totalBytes > AES_OP_SIZE_LIMIT) {
		fprintf(stderr,"** Size limit of %d reached, %d bytes to process\n", AES_OP_SIZE_LIMIT, totalBytes);
	}
	if (0 != paramPointer) {
		fprintf(stderr,"** Launch Single Shot z_kmc_native, calling %p\n", cbcOp.z_kmc_native);
	} else {
		fprintf(stderr,"** Launch Single Shot cbcOp.CIPHER_op, calling %p\n", cbcOp.CIPHER_op);
	}
#endif /* CRYPTO_DEBUG */
	if (totalBytes <= AES_OP_SIZE_LIMIT) {
		if (0 != paramPointer) {
			result = (*(cbcOp.z_kmc_native))(inputAddress, outputAddress, totalBytes, (long)paramPointer, mode);
		} else {
			result = (*(cbcOp.CIPHER_op))((void*)((intptr_t)iccContextId), (void*)((intptr_t)iccCipherId), inputAddress, totalBytes, outputAddress, needsReinit);
		}
	} else {
		/* the requested operation is too large to be done in a single op since that would acquire VMAccess for too long
		 * so the operation is broken up */
		int returnCode = 0;
		jint currentBytes = 0;
		jint bytesToOp = totalBytes;

		while (0 != bytesToOp) {
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"** %d bytes out of %d\n", bytesToOp, totalBytes);
#endif /* CRYPTO_DEBUG */
			currentBytes = ((bytesToOp - AES_OP_SIZE_LIMIT) >= 0) ? AES_OP_SIZE_LIMIT : bytesToOp;
			if (0 != paramPointer) {
				returnCode = (*(cbcOp.z_kmc_native))(inputAddress, outputAddress, currentBytes, (long)paramPointer, mode);
			} else {
				if (bytesToOp == totalBytes) {
#if defined(CRYPTO_DEBUG)
					fprintf(stderr,"** Launch first cbcOp.CIPHER_op with %d bytes, (%p -> %p)\n", (int)currentBytes, inputAddress, outputAddress);
#endif /* CRYPTO_DEBUG */
					/* first call, pass the needsReinit flag and call update */
					returnCode = (*cbcOpUpdate)((void*)((intptr_t)iccContextId), (void*)((intptr_t)iccCipherId), inputAddress, currentBytes, outputAddress, needsReinit);
				} else if ((bytesToOp - currentBytes) > 0) {
#if defined(CRYPTO_DEBUG)
					fprintf(stderr,"** Launch subseq but not final cbcOp.CIPHER_op with %d bytes, (%p -> %p)\n", (int)currentBytes, inputAddress, outputAddress);
#endif /* CRYPTO_DEBUG */
					/* subsequent but not final call, call update and do not pass needsReinit flag */
					returnCode = (*cbcOpUpdate)((void*)((intptr_t)iccContextId), (void*)((intptr_t)iccCipherId), inputAddress, currentBytes, outputAddress, JNI_FALSE);
				} else {
#if defined(CRYPTO_DEBUG)
					fprintf(stderr,"** Launch final cbcOp.CIPHER_op with %d bytes, (%p -> %p)\n", (int)currentBytes, inputAddress, outputAddress);
#endif /* CRYPTO_DEBUG */
					/* final call, call final and do not pass needsReinit flag */
					returnCode = (*(cbcOp.CIPHER_op))((void*)((intptr_t)iccContextId), (void*)((intptr_t)iccCipherId), inputAddress, currentBytes, outputAddress, JNI_FALSE);
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
				
				vmFuncs->internalReleaseVMAccess(currentThread);
				vmFuncs->internalAcquireVMAccess(currentThread);
				in = J9_JNI_UNWRAP_REFERENCE(input);
				out = J9_JNI_UNWRAP_REFERENCE(output);
			}
			inputIndex += currentBytes;
			outputIndex += returnCode;
			inputAddress = J9JAVAARRAY_EA(currentThread, in, inputIndex, U_8);
			outputAddress = J9JAVAARRAY_EA(currentThread, out, outputIndex, U_8);
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"** Recompute Addresses : %p %p %d %d\n", inputAddress, outputAddress, inputIndex, outputIndex);
#endif /* CRYPTO_DEBUG */
			bytesToOp -= currentBytes;
		}
	}
	return result;
}

/*
 * discontiguousAESLaunch handles AES operations when the input and/or output arrays are in arraylet form and therefore discontiguous.
 * Temporary buffers are used for input and output for the AES operations since AES operations do not support fragmented output arrays.
 *
 * @param[in] currentThread - current thread
 * @param[in] iccContextId - native library context
 * @param[in] iccCipherId - cipher state context
 * @param[in] input - input byte array
 * @param[in] inputOffset - starting offset for input
 * @param[in] inputLen - length of input data
 * @param[in] output - output byte array
 * @param[in] outputOffset - starting offset for output
 * @param[in] needsReinit - flag indicating if operation requires a reinit
 * @param[in] cbcOpUpdate - function pointer for AES Update needed when a large input has to be broken up to segments
 * @param[in] cbcOp - function pointer for requested AES operation 
 * @param[in] paramPointer - pointer to internal state used by z14 and higher Z-CPUs
 * @param[in] mode - mode used by z14 and higher Z-CPUs
 *
 * @return - if the value is negative, this is a returned error code. otherwise the value is the return length
 */
static VMINLINE jint
discontiguousAESLaunch(J9VMThread *currentThread, jlong iccContextId, jlong iccCipherId, jobject input, jint inputOffset, jint inputLen, jobject output, jint outputOffset, jboolean needsReinit, CIPHER_t* cbcOpUpdate, cryptoFunc_t cbcOp, jlong paramPointer, jint mode)
{
	jint ret = 0;
	j9object_t unwrap_out = J9_JNI_UNWRAP_REFERENCE(output);
	I_32 outputArraySize = J9INDEXABLEOBJECT_SIZE(currentThread, unwrap_out);
	PORT_ACCESS_FROM_VMC(currentThread);

#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** Enter discontiguousAESLaunch %d + %d -> %d + %d (%lld %lld)\n", inputLen, inputOffset, outputArraySize, outputOffset, (long long int)iccContextId, (long long int)iccCipherId);
#endif /* CRYPTO_DEBUG */
	/* allocate native buffers to be used to directly interact with crypto API */
	void* inputNativePtr = j9mem_allocate_memory(inputLen, J9MEM_CATEGORY_VM_JCL);
	void* outputNativePtr = j9mem_allocate_memory(outputArraySize, J9MEM_CATEGORY_VM_JCL);

	if ((NULL != inputNativePtr) && (NULL != outputNativePtr)) {
		/* copy from input arraylets to native buffer */
		VM_ArrayCopyHelpers::memcpyFromArray(currentThread, J9_JNI_UNWRAP_REFERENCE(input), inputOffset, inputLen, inputNativePtr);
		/* do crypto operation */
		if (0 != paramPointer) {
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"** Launch Single Shot z_kmc_native with %d bytes, calling %p from %p to %p\n", (int)inputLen, cbcOp.z_kmc_native, inputNativePtr, outputNativePtr);
#endif /* CRYPTO_DEBUG */
			ret = (*(cbcOp.z_kmc_native))(inputNativePtr, outputNativePtr, inputLen, (long)paramPointer, mode);
		} else {
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"** Launch Single Shot cbcOp.CIPHER_op with %d bytes, calling %p from %p to %p\n", (int)inputLen, cbcOp.CIPHER_op, inputNativePtr, outputNativePtr);
#endif /* CRYPTO_DEBUG */
			ret = (*(cbcOp.CIPHER_op))( (void*)((intptr_t)iccContextId), (void*)((intptr_t)iccCipherId), inputNativePtr, inputLen, outputNativePtr, needsReinit);
		}
#if defined(CRYPTO_DEBUG)
		fprintf(stderr,"*** Done. Copy back and free %d\n", ret);
#endif /* CRYPTO_DEBUG */
		/* negative ret means an error code, don't copy in that case */
		if (ret > 0) {
			/* copy back from output native buffer to output arraylets */
			VM_ArrayCopyHelpers::memcpyToArray(currentThread, J9_JNI_UNWRAP_REFERENCE(output), outputOffset, ret, outputNativePtr);
		}
		/* release input and output native buffers */
		j9mem_free_memory(outputNativePtr);
		j9mem_free_memory(inputNativePtr);
	} else {
#if defined(CRYPTO_DEBUG)
		fprintf(stderr,"** Failed to allocate memory for discontiguousAESLaunch\n");
#endif /* CRYPTO_DEBUG */
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
 *
 * @param[in] currentThread - current thread
 * @param[in] iccContextId - native library context
 * @param[in] iccCipherId - cipher state context
 * @param[in] input - input byte array
 * @param[in] inputOffset - starting offset for input
 * @param[in] inputLen - length of input data
 * @param[in] output - output byte array
 * @param[in] outputOffset - starting offset for output
 * @param[in] needsReinit - flag indicating if operation requires a reinit
 * @param[in] cbcOpUpdate - function pointer for AES Update needed when a large input has to be broken up to segments
 * @param[in] cbcOp - function pointer for requested AES operation 
 * @param[in] paramPointer - pointer to internal state used by z14 and higher Z-CPUs
 * @param[in] mode - mode used by z14 and higher Z-CPUs
 *
 * @return - if the value is negative, this is a returned error code. otherwise the value is the return length
 */
static VMINLINE jint
AES(J9VMThread *currentThread, jlong iccContextId, jlong iccCipherId, jobject input, jint inputOffset, jint inputLen, jobject output, jint outputOffset, jboolean needsReinit, CIPHER_t* cbcOpUpdate, cryptoFunc_t cbcOp, jlong paramPointer, jint mode)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** AESLaunch: %d %d %d %d %p %p %p ---- ", inputOffset, inputLen, outputOffset, needsReinit, cbcOpUpdate, input, output);
#endif /* CRYPTO_DEBUG */
	j9object_t unwrap_out = J9_JNI_UNWRAP_REFERENCE(output);
	I_32 outputArraySize = J9INDEXABLEOBJECT_SIZE(currentThread, unwrap_out);
	bool isContInput = isCryptoArrayContiguous(currentThread, inputOffset, inputLen);
	bool isContOutput = isCryptoArrayContiguous(currentThread, outputOffset, outputArraySize);
	/* The if statement below determines if contiguous array or discontiguous array processing is required.
	 * there are four inputs: 1. is input array contiguous
	 * 			  2. is output array contiguous
	 * 			  3. is input length greater than zero
	 * 			  4. is output length greater than zero
	 * Note that the input length is passed from the JCL code so incorrect value could cause a crash in the crypto. lib - in which case the JCL code needs to be fixed
	 */ 			  
	bool isContiguous = ((((inputLen > 0) && (outputArraySize > 0)) && (isContInput && isContOutput)) /* lengths are both greater than zero, contg. proc. if both arrays contg.       */
				|| (((inputLen <= 0) && (outputArraySize > 0)) && isContOutput)           /* only output len greater than zero, contg. proc. if output array is contg.    */
				|| (((inputLen > 0) && (outputArraySize <= 0)) && isContInput)            /* only input length greater than zero, contg. proc. if input array is contg.   */
				|| (((inputLen <= 0) && (outputArraySize <= 0))));                        /* both input and output arrays less than zero, send to contg. array processing */
	jint result;
#if defined(CRYPTO_DEBUG)
	if (isContiguous) {
		fprintf(stderr,"contiguous %d\n", outputArraySize);
	} else {
		fprintf(stderr,"discontiguous: %d\n", outputArraySize);
	}
#endif /* CRYPTO_DEBUG */

	if (isContiguous) {
		result = AESLaunch(currentThread, iccContextId, iccCipherId, input, inputOffset, inputLen, output, outputOffset, needsReinit, cbcOpUpdate, cbcOp, paramPointer, mode);
	} else {
		result = discontiguousAESLaunch(currentThread, iccContextId, iccCipherId, input, inputOffset, inputLen, output, outputOffset, needsReinit, cbcOpUpdate, cbcOp, paramPointer, mode);
	}
	return result;
}

/*
 * Main entry point into HMAC routine, here it is checked if the input and output arrays are discontigious, if they are
 * the crypto routine is called on each arraylet.
 *
 * This routine handles the HMAC update operation. The input and output are always byte arrays.
 *
 * If the input data is very large, the  VMAccess lock may be acquired for too extended a duration. In that case,
 * VMAccess is released and re-acquired after a fixed number of bytes have been processed to allow the GC to operate.
 *
 * The maxinum number of bytes to be processed at a single operation was selected as follows: VMAccess should not be acquired for no more than 0.1ms.
 * Benchmarks indicate current performance (2020) is at 140 MB/s for HMAC-SHA256 update at 16 threads on AIX, PPCLE and X86. 200 MB/s for Z platforms. 
 * Other scenarios & platforms are all faster. Using 140 MB/s as worst case performance, we have ~ 16384 bytes processed at 0.1ms
 *
 * there are no error checking implemented here on input and output buffers, all errors are handled by JCL code.
 *
 * @param[in] currentThread - current thread
 * @param[in] iccContextId - native library context
 * @param[in] hmacId - pointer to native hmac object state context
 * @param[in] key - key byte array
 * @param[in] keyLength - length of key data
 * @param[in] input - input byte array
 * @param[in] inputOffset - starting offset for input
 * @param[in] needInit - flag indicating if operation requires a reinit
 *
 * @return - if the value is negative, this is a returned error code. otherwise the value is the return length
 */
static VMINLINE jint
hmacUpdate(J9VMThread *currentThread, jlong iccContextId, jlong hmacId, jobject key, jint keyLength, jobject input, jint inputOffset, jint inputLength, jboolean needInit)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** hmacUpdate: %p %d %p %d %d %d ---- ", key, keyLength, input, inputOffset, inputLength, needInit);
#endif /* CRYPTO_DEBUG */
	cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniCryptoFunctions;
	HMAC_update_t* HMAC_update = cryptFuncs->HMAC_update;
	bool keyIsCont = isCryptoArrayContiguous(currentThread, 0, keyLength);
	bool inputIsCont = isCryptoArrayContiguous(currentThread, inputOffset, inputLength);
	PORT_ACCESS_FROM_VMC(currentThread);
	/* The if statement below determines if contiguous array or discontiguous array processing is required.
	 * there are four inputs: 1. is key array contiguous
	 * 			  2. is hmac array contiguous
	 * 			  3. is key length greater than zero
	 * 			  4. is hmac length greater than zero
	 * Note that the input length is passed from the JCL code so incorrect value could cause a crash in the crypto. lib - in which case the JCL code needs to be fixed
	 */ 			  
	bool isContiguous = ((((keyLength > 0) && (inputLength > 0)) && (keyIsCont && inputIsCont)) /* lengths are both greater than zero, contg. proc. if both arrays contg.       */
				|| (((keyLength <= 0) && (inputLength > 0)) && inputIsCont)         /* only output len greater than zero, contg. proc. if output array is contg.    */
				|| (((keyLength > 0)  && (inputLength <= 0)) && keyIsCont)          /* only input length greater than zero, contg. proc. if input array is contg.   */
				|| (((keyLength <= 0) && (inputLength <= 0))));                     /* both input and output arrays less than zero, send to contg. array processing */
	jint result = -1;

#if defined(CRYPTO_DEBUG)
	if (isContiguous) {
		fprintf(stderr,"hmacUpdate contiguous\n");
	} else {
		fprintf(stderr,"hmacUpdate discontiguous\n");
	}
#endif /* CRYPTO_DEBUG */
	if (isContiguous) {
		j9object_t keyUnwrap = J9_JNI_UNWRAP_REFERENCE(key);
		j9object_t inputUnwrap = J9_JNI_UNWRAP_REFERENCE(input);
		void* keyAddress = J9JAVAARRAY_EA(currentThread, keyUnwrap, 0, U_8);
		void* inputAddress = J9JAVAARRAY_EA(currentThread, inputUnwrap, inputOffset, U_8);
		/* contiguous array */
		if (inputLength <= HMAC_OP_SIZE_LIMIT) {
			result = (*HMAC_update)((void*)((intptr_t)iccContextId), (void*)((intptr_t)hmacId), keyAddress, keyLength, inputAddress, inputLength, needInit);
		} else {
			/* the requested operation is too large to be done in a single op since that would acquire VMAccess for too long
			 * so the operation is broken up */
			int returnCode = 0;
			jint currentBytes = 0;
			jint bytesToOp = inputLength;
			int inputIndex = inputOffset;
			result = 0;
			while (0 != bytesToOp) {
#if defined(CRYPTO_DEBUG)
				fprintf(stderr,"** %d bytes out of %d\n", bytesToOp, inputLength);
#endif /* CRYPTO_DEBUG */
				currentBytes = ((bytesToOp - HMAC_OP_SIZE_LIMIT) >= 0) ? HMAC_OP_SIZE_LIMIT : bytesToOp;
				if (bytesToOp == inputLength) {
#if defined(CRYPTO_DEBUG)
					fprintf(stderr,"** Launch first hmacUpdate with %d bytes, (%p, %p)\n", (int)currentBytes, inputAddress, keyAddress);
#endif /* CRYPTO_DEBUG */
					/* first call, pass the needsReinit flag and call update */
					returnCode = (*HMAC_update)((void*)((intptr_t)iccContextId), (void*)((intptr_t)hmacId), keyAddress, keyLength, inputAddress, currentBytes, needInit);
				} else if ((bytesToOp - currentBytes) > 0) {
#if defined(CRYPTO_DEBUG)
					fprintf(stderr,"** Launch subseq but not final hmacUpdate with %d bytes, (%p, %p)\n", (int)currentBytes, inputAddress, keyAddress);
#endif /* CRYPTO_DEBUG */
					/* subsequent but not final call, call update and do not pass needInit flag */
					returnCode = (*HMAC_update)((void*)((intptr_t)iccContextId), (void*)((intptr_t)hmacId), keyAddress, keyLength, inputAddress, currentBytes, JNI_FALSE);
				} else {
#if defined(CRYPTO_DEBUG)
					fprintf(stderr,"** Launch final hmacUpdate with %d bytes, (%p, %p)\n", (int)currentBytes, inputAddress, keyAddress);
#endif /* CRYPTO_DEBUG */
					/* final call, call final and do not pass needsReinit flag */
					returnCode = (*HMAC_update)((void*)((intptr_t)iccContextId), (void*)((intptr_t)hmacId), keyAddress, keyLength, inputAddress, currentBytes, JNI_FALSE);
					/* operation done, return */
					break;
				}
				if (returnCode < 0) {
					/* Crypto returned error, returning immediately */
					result = returnCode;
					break;
				}
				/*
				 * release VMAccess to allow GC to work if needed
				 * recover input pointers afterwards
				 */
				if (J9_ARE_ANY_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_ANY)) {
					J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;
					
					vmFuncs->internalReleaseVMAccess(currentThread);
					vmFuncs->internalAcquireVMAccess(currentThread);
					inputUnwrap = J9_JNI_UNWRAP_REFERENCE(input);
					keyUnwrap = J9_JNI_UNWRAP_REFERENCE(key);
				}
				inputIndex += currentBytes;
				inputAddress = J9JAVAARRAY_EA(currentThread, inputUnwrap, inputIndex, U_8);
				keyAddress = J9JAVAARRAY_EA(currentThread, keyUnwrap, 0, U_8);
#if defined(CRYPTO_DEBUG)
				fprintf(stderr,"** Recompute input addresses : %p %d\n", inputAddress, inputIndex);
#endif /* CRYPTO_DEBUG */
				bytesToOp -= currentBytes;
			}
		}
	} else {
		/* discontiguous arrays */
		/* allocate native buffers to be used to directly interact with crypto API */
		void* keyNativePtr = j9mem_allocate_memory(keyLength, J9MEM_CATEGORY_VM_JCL);
		void* inputNativePtr = j9mem_allocate_memory(inputLength, J9MEM_CATEGORY_VM_JCL);

		if ((NULL != keyNativePtr) && (NULL != inputNativePtr)) {
			/* copy from input arraylets to native buffer */
			VM_ArrayCopyHelpers::memcpyFromArray(currentThread, J9_JNI_UNWRAP_REFERENCE(key), 0, keyLength, keyNativePtr);
			VM_ArrayCopyHelpers::memcpyFromArray(currentThread, J9_JNI_UNWRAP_REFERENCE(input), inputOffset, inputLength, inputNativePtr);
			/* do crypto operation */
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"** Launch discontig HMAC_update with %d bytes, calling %p from %p (%p)\n", (int)inputLength, HMAC_update, inputNativePtr, keyNativePtr);
#endif /* CRYPTO_DEBUG */
			result = (*(HMAC_update))( (void*)((intptr_t)iccContextId), (void*)((intptr_t)hmacId), keyNativePtr, keyLength, inputNativePtr, inputLength, needInit);
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"*** Done. Copy back and free %d\n", result);
#endif /* CRYPTO_DEBUG */
			/* release input and output native buffers */
			j9mem_free_memory(keyNativePtr);
			j9mem_free_memory(inputNativePtr);
		} else {
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"** Failed to allocate memory for discontiguous hmacUpdate\n");
#endif /* CRYPTO_DEBUG */
			/* throw OOM exception, unable to allocate buffer */
			result = -1;
			setNativeOutOfMemoryError(currentThread, J9NLS_VM_NATIVE_OOM);
		}
	}
	return result;
}

/*
 * Main entry point into HMAC final routine, here it is checked if the input and output arrays are discontigious, if they are
 * the crypto routine is called on each arraylet.
 *
 * @param[in] currentThread - current thread
 * @param[in] iccContextId - native library context
 * @param[in] hmacId - pointer to native hmac state context
 * @param[in] key - key byte array
 * @param[in] keyLength - length of key data
 * @param[in] hmac - input byte array
 * @param[in] needInit - flag indicating if operation requires a reinit
 *
 * @return - if the value is negative, this is a returned error code. otherwise the value is the return length
 */
static VMINLINE jint
hmacFinal(J9VMThread *currentThread, jlong iccContextId, jlong hmacId, jobject key, jint keyLength, jobject hmac, jboolean needInit)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** hmacFinal: %d %d %p %p ---- ", keyLength, needInit, key, hmac);
#endif /* CRYPTO_DEBUG */
	cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniCryptoFunctions;
	HMAC_doFinal_t* HMAC_doFinal = cryptFuncs->HMAC_doFinal;
	j9object_t hmacUnwrap = J9_JNI_UNWRAP_REFERENCE(hmac);
	I_32 hmacArraySize = J9INDEXABLEOBJECT_SIZE(currentThread, hmacUnwrap);
	bool keyIsCont = isCryptoArrayContiguous(currentThread, 0, keyLength);
	bool hmacIsCont = isCryptoArrayContiguous(currentThread, 0, hmacArraySize);
	PORT_ACCESS_FROM_VMC(currentThread);
	/* The if statement below determines if contiguous array or discontiguous array processing is required.
	 * there are four inputs: 1. is key array contiguous
	 * 			  2. is hmac array contiguous
	 * 			  3. is key length greater than zero
	 * 			  4. is hmac length greater than zero
	 * Note that the input length is passed from the JCL code so incorrect value could cause a crash in the crypto. lib - in which case the JCL code needs to be fixed
	 */ 			  
	bool isContiguous = ((((keyLength > 0) && (hmacArraySize > 0)) && (keyIsCont && hmacIsCont)) /* lengths are both greater than zero, contg. proc. if both arrays contg.       */
				|| (((keyLength <= 0) && (hmacArraySize > 0)) && hmacIsCont)         /* only output len greater than zero, contg. proc. if output array is contg.    */
				|| (((keyLength > 0)  && (hmacArraySize <= 0)) && keyIsCont)         /* only input length greater than zero, contg. proc. if input array is contg.   */
				|| (((keyLength <= 0) && (hmacArraySize <= 0))));                    /* both input and output arrays less than zero, send to contg. array processing */
	jint result = -1;
#if defined(CRYPTO_DEBUG)
	if (isContiguous) {
		fprintf(stderr,"contiguous %d\n", hmacArraySize);
	} else {
		fprintf(stderr,"discontiguous: %d\n", hmacArraySize);
	}
#endif /* CRYPTO_DEBUG */

	if (isContiguous) {
		/* contiguous array */
		j9object_t keyUnwrap = J9_JNI_UNWRAP_REFERENCE(key);
		void* keyAddress = J9JAVAARRAY_EA(currentThread, keyUnwrap, 0, U_8);
		void* hmacAddress = J9JAVAARRAY_EA(currentThread, hmacUnwrap, 0, U_8);
	
#if defined(CRYPTO_DEBUG)
		fprintf(stderr,"contiguous hmacFinal %p + %d -> %p (%lld %lld)\n", keyAddress, keyLength, hmacAddress, (long long int)iccContextId, (long long int)hmacId);
#endif /* CRYPTO_DEBUG */
		result = (*(HMAC_doFinal))( (void*)((intptr_t)iccContextId), (void*)((intptr_t)hmacId), keyAddress, keyLength, hmacAddress, needInit);
	} else {
		/* discontiguous arrays */
#if defined(CRYPTO_DEBUG)
		fprintf(stderr,"** Enter discontiguous hmacFinal %d -> %d (%lld %lld)\n", keyLength, hmacArraySize, (long long int)iccContextId, (long long int)hmacId);
#endif /* CRYPTO_DEBUG */
		/* allocate native buffers to be used to directly interact with crypto API */
		void* keyNativePtr = j9mem_allocate_memory(keyLength, J9MEM_CATEGORY_VM_JCL);
		void* hmacNativePtr = j9mem_allocate_memory(hmacArraySize, J9MEM_CATEGORY_VM_JCL);

		if ((NULL != keyNativePtr) && (NULL != hmacNativePtr)) {
			/* copy from input arraylets to native buffer */
			VM_ArrayCopyHelpers::memcpyFromArray(currentThread, J9_JNI_UNWRAP_REFERENCE(key), 0, keyLength, keyNativePtr);
			/* do crypto operation */
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"** Launch discontig hmacFinal with %d bytes, calling %p from %p to %p\n", (int)keyLength, HMAC_doFinal, keyNativePtr, hmacNativePtr);
#endif /* CRYPTO_DEBUG */
			result = (*(HMAC_doFinal))( (void*)((intptr_t)iccContextId), (void*)((intptr_t)hmacId), keyNativePtr, keyLength, hmacNativePtr, needInit);
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"*** Done. Copy back and free %d\n", result);
#endif /* CRYPTO_DEBUG */
			/* negative ret means an error code, don't copy in that case */
			if (result > 0) {
				/* copy back from output native buffer to output arraylets */
				VM_ArrayCopyHelpers::memcpyToArray(currentThread, J9_JNI_UNWRAP_REFERENCE(hmac), 0, result, hmacNativePtr);
			}
			/* release input and output native buffers */
			j9mem_free_memory(keyNativePtr);
			j9mem_free_memory(hmacNativePtr);
		} else {
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"** Failed to allocate memory for hmacFinal\n");
#endif /* CRYPTO_DEBUG */
			/* throw OOM exception, unable to allocate buffer */
			result = -1;
			setNativeOutOfMemoryError(currentThread, J9NLS_VM_NATIVE_OOM);
		}
	}
	/* return output size or error code */
	return result;
}

/*
 * Main entry point into DIGEST update routine, here it is checked if the input and output arrays are discontigious, if they are
 * the crypto routine is called on each arraylet.
 *
 * @param[in] currentThread - current thread
 * @param[in] iccContextId - native library context
 * @param[in] digetId - digest state context
 * @param[in] input - byte array input
 * @param[in] inputOffset
 * @param[in] inputLength
 * @return - if the value is negative, this is a returned error code. otherwise the value is the return length
 */
static VMINLINE jint
digestUpdate(J9VMThread *currentThread, jlong iccContextId, jlong digestId, jobject input, jint inputOffset, jint inputLength)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** digestUpdate: %p %d %d ---- ", input, inputOffset, inputLength);
#endif /* CRYPTO_DEBUG */
	cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniCryptoFunctions;
	DIGEST_update_t* DIGEST_update = cryptFuncs->DIGEST_update;
	bool inputIsCont = isCryptoArrayContiguous(currentThread, inputOffset, inputLength);
	PORT_ACCESS_FROM_VMC(currentThread);
	/* The if statement below determines if contiguous array or discontiguous array processing is required.
	 * Note that the input length is passed from the JCL code so incorrect value could cause a crash in the crypto. lib - in which case the JCL code needs to be fixed
	 */ 			  
	bool isContiguous = ((((inputLength > 0)) && ( inputIsCont)) || (inputLength <= 0));
	jint result = -1;

#if defined(CRYPTO_DEBUG)
	if (isContiguous) {
		fprintf(stderr,"digestUpdate contiguous\n");
	} else {
		fprintf(stderr,"digestUpdate discontiguous\n");
	}
#endif /* CRYPTO_DEBUG */
	if (isContiguous) {
		j9object_t inputUnwrap = J9_JNI_UNWRAP_REFERENCE(input);
		void* inputAddress = J9JAVAARRAY_EA(currentThread, inputUnwrap, inputOffset, U_8);
		/* contiguous array */
		if (inputLength <= DIGEST_OP_SIZE_LIMIT) {
			result = (*DIGEST_update)((void*)((intptr_t)iccContextId), (void*)((intptr_t)digestId), inputAddress, inputLength);
		} else {
			/* the requested operation is too large to be done in a single op since that would acquire VMAccess for too long
			 * so the operation is broken up */
			int returnCode = 0;
			jint currentBytes = 0;
			jint bytesToOp = inputLength;
			int inputIndex = inputOffset;
			result = 0;
			while (0 != bytesToOp) {
#if defined(CRYPTO_DEBUG)
				fprintf(stderr,"** %d bytes out of %d\n", bytesToOp, inputLength);
#endif /* CRYPTO_DEBUG */
				currentBytes = ((bytesToOp - DIGEST_OP_SIZE_LIMIT) >= 0) ? DIGEST_OP_SIZE_LIMIT : bytesToOp;
				if (bytesToOp == inputLength) {
#if defined(CRYPTO_DEBUG)
					fprintf(stderr,"** Launch first digestUpdate with %d bytes, (%p)\n", (int)currentBytes, inputAddress);
#endif /* CRYPTO_DEBUG */
					/* first call */
					returnCode = (*DIGEST_update)((void*)((intptr_t)iccContextId), (void*)((intptr_t)digestId), inputAddress, currentBytes);
				} else if ((bytesToOp - currentBytes) > 0) {
#if defined(CRYPTO_DEBUG)
					fprintf(stderr,"** Launch subseq but not final digestUpdate with %d bytes, (%p)\n", (int)currentBytes, inputAddress);
#endif /* CRYPTO_DEBUG */
					/* subsequent but not final call, call update */
					returnCode = (*DIGEST_update)((void*)((intptr_t)iccContextId), (void*)((intptr_t)digestId), inputAddress, currentBytes);
				} else {
#if defined(CRYPTO_DEBUG)
					fprintf(stderr,"** Launch final digestUpdate with %d bytes, (%p)\n", (int)currentBytes, inputAddress);
#endif /* CRYPTO_DEBUG */
					/* final call */
					returnCode = (*DIGEST_update)((void*)((intptr_t)iccContextId), (void*)((intptr_t)digestId), inputAddress, currentBytes);
					/* operation done, return */
					break;
				}
				if (returnCode < 0) {
					/* Crypto returned error, returning immediately */
					result = returnCode;
					break;
				}
				/*
				 * release VMAccess to allow GC to work if needed
				 * recover input pointers afterwards
				 */
				if (J9_ARE_ANY_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_ANY)) {
					J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;
					
					vmFuncs->internalReleaseVMAccess(currentThread);
					vmFuncs->internalAcquireVMAccess(currentThread);
					inputUnwrap = J9_JNI_UNWRAP_REFERENCE(input);
				}
				inputIndex += currentBytes;
				inputAddress = J9JAVAARRAY_EA(currentThread, inputUnwrap, inputIndex, U_8);
#if defined(CRYPTO_DEBUG)
				fprintf(stderr,"** Recompute input addresses : %p %d\n", inputAddress, inputIndex);
#endif /* CRYPTO_DEBUG */
				bytesToOp -= currentBytes;
			}
		}
	} else {
		/* discontiguous arrays */
		/* allocate native buffers to be used to directly interact with crypto API */
		void* inputNativePtr = j9mem_allocate_memory(inputLength, J9MEM_CATEGORY_VM_JCL);

		if (NULL != inputNativePtr) {
			/* copy from input arraylets to native buffer */
			VM_ArrayCopyHelpers::memcpyFromArray(currentThread, J9_JNI_UNWRAP_REFERENCE(input), inputOffset, inputLength, inputNativePtr);
			/* do crypto operation */
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"** Launch discontig DIGEST_update with %d bytes, calling %p using input %p\n", (int)inputLength, DIGEST_update, inputNativePtr);
#endif /* CRYPTO_DEBUG */
			result = (*(DIGEST_update))( (void*)((intptr_t)iccContextId), (void*)((intptr_t)digestId), inputNativePtr, inputLength);
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"*** Done. Copy back and free %d\n", result);
#endif /* CRYPTO_DEBUG */
			/* release input native buffer */
			j9mem_free_memory(inputNativePtr);
		} else {
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"** Failed to allocate memory for discontiguous hmacUpdate\n");
#endif /* CRYPTO_DEBUG */
			/* throw OOM exception, unable to allocate buffer */
			result = -1;
			setNativeOutOfMemoryError(currentThread, J9NLS_VM_NATIVE_OOM);
		}
	}
	return result;
}

/*
 * Main entry point into DIGEST_digest routine, here it is checked if the output array is discontigious and handles
 * that appropriately. This routine computes the output digest generated from the input provided by previous
 * update routine invocations.
 *
 * @param[in] currentThread - current thread
 * @param[in] iccContextId - native library context
 * @param[in] digetId - digest state context
 * @param[in] digest - properly sized byte array for the output digest
 * @return - if the value is negative, this is a returned error code. otherwise the value is the return length
 */
static VMINLINE jint
digestDigest(J9VMThread *currentThread, jlong iccContextId, jlong digestId, jobject digest)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** digestDigest: %p ---- ", digest);
#endif /* CRYPTO_DEBUG */
	cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniCryptoFunctions;
	DIGEST_digest_t* DIGEST_digest = cryptFuncs->DIGEST_digest;
	j9object_t digestUnwrap = J9_JNI_UNWRAP_REFERENCE(digest);
	I_32 digestArraySize = J9INDEXABLEOBJECT_SIZE(currentThread, digestUnwrap);
	bool digestIsCont = isCryptoArrayContiguous(currentThread, 0, digestArraySize);
	PORT_ACCESS_FROM_VMC(currentThread);
	/* The if statement below determines if contiguous array or discontiguous array processing is required.
	 * Note that the input length is passed from the JCL code so incorrect value could cause a crash in the crypto. lib - in which case the JCL code needs to be fixed
	 */ 			  
	bool isContiguous = ((digestArraySize > 0) && digestIsCont) || (digestArraySize <= 0);
	jint result = -1;
#if defined(CRYPTO_DEBUG)
	if (isContiguous) {
		fprintf(stderr,"contiguous %d\n", digestArraySize);
	} else {
		fprintf(stderr,"discontiguous: %d\n", digestArraySize);
	}
#endif /* CRYPTO_DEBUG */

	if (isContiguous) {
		/* contiguous array */
		void* digestAddress = J9JAVAARRAY_EA(currentThread, digestUnwrap, 0, U_8);
	
#if defined(CRYPTO_DEBUG)
		fprintf(stderr,"contiguous digestDigest -> %p (%lld %lld)\n", digestAddress, (long long int)iccContextId, (long long int)digestId);
#endif /* CRYPTO_DEBUG */
		result = (*(DIGEST_digest))( (void*)((intptr_t)iccContextId), (void*)((intptr_t)digestId), digestAddress);
	} else {
		/* discontiguous arrays */
#if defined(CRYPTO_DEBUG)
		fprintf(stderr,"** Enter discontiguous digestDigest -> %d (%lld %lld)\n", digestArraySize, (long long int)iccContextId, (long long int)digestId);
#endif /* CRYPTO_DEBUG */
		/* allocate native buffers to be used to directly interact with crypto API */
		void* digestNativePtr = j9mem_allocate_memory(digestArraySize, J9MEM_CATEGORY_VM_JCL);

		if (NULL != digestNativePtr) {
			/* do crypto operation */
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"** Launch discontig digestDigest using %p\n", digestNativePtr);
#endif /* CRYPTO_DEBUG */
			result = (*(DIGEST_digest))( (void*)((intptr_t)iccContextId), (void*)((intptr_t)digestId), digestNativePtr);
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"*** Done. Copy back and free %d\n", result);
#endif /* CRYPTO_DEBUG */
			/* negative ret means an error code, don't copy in that case */
			if (result > 0) {
				/* copy back from output native buffer to output arraylets */
				VM_ArrayCopyHelpers::memcpyToArray(currentThread, J9_JNI_UNWRAP_REFERENCE(digest), 0, result, digestNativePtr);
			}
			/* release digest native buffer */
			j9mem_free_memory(digestNativePtr);
		} else {
#if defined(CRYPTO_DEBUG)
			fprintf(stderr,"** Failed to allocate memory for digestDigest\n");
#endif /* CRYPTO_DEBUG */
			/* throw OOM exception, unable to allocate buffer */
			result = -1;
			setNativeOutOfMemoryError(currentThread, J9NLS_VM_NATIVE_OOM);
		}
	}
	/* return output size or error code */
	return result;
}

/* com.ibm.crypto.plus.provider.icc: static public native int CIPHER_encryptFinal(long iccContextId, long iccCipherId, byte[] input, int inOffset, int inLen, byte[] ciphertext, int ciphertextOffset, boolean needsReinit) throws ICCException; */
jint JNICALL
Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_encryptFinal(J9VMThread *currentThread, jlong iccContextId, jlong iccCipherId, jobject plaintext, jint plaintextOffset, jint plaintextLen, jobject ciphertext, jint ciphertextOffset, jboolean needsReinit)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** CIPHER_encryptFinal operation\n");
#endif /* CRYPTO_DEBUG */
	bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniCryptoFunctions);
	if (!cryptoLibLoaded) {
		loadCryptoLib(currentThread);
	}
	cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniCryptoFunctions;
	return AES(currentThread, iccContextId, iccCipherId, plaintext, plaintextOffset, plaintextLen, ciphertext, ciphertextOffset, needsReinit, cryptFuncs->CIPHER_encryptUpdate, cryptFuncs->CIPHER_encryptFinalPtr, 0, 0);
}

/* com.ibm.crypto.plus.provider.icc: static public native int CIPHER_decryptFinal(long iccContextId, long iccCipherId, byte[] ciphertext, int cipherOffset, int cipherLen, byte[] plaintext, int plaintextOffset, boolean needsReinit) throws ICCException; */
jint JNICALL
Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_decryptFinal(J9VMThread *currentThread, jlong iccContextId, jlong iccCipherId, jobject ciphertext, jint ciphertextOffset, jint ciphertextLen, jobject plaintext, jint plaintextOffset, jboolean needsReinit)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** CIPHER_decryptFinal operation\n");
#endif /* CRYPTO_DEBUG */
	bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniCryptoFunctions);
	if (!cryptoLibLoaded) {
		loadCryptoLib(currentThread);
	}
	cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniCryptoFunctions;
	return AES(currentThread, iccContextId, iccCipherId, ciphertext, ciphertextOffset, ciphertextLen, plaintext, plaintextOffset, needsReinit, cryptFuncs->CIPHER_decryptUpdate, cryptFuncs->CIPHER_decryptFinalPtr, 0, 0);
}

/* com.ibm.crypto.plus.provider.icc: static public native int CIPHER_encryptUpdate(long iccContextId, long iccCipherId, byte[] plaintext, int plaintextOffset, int plaintextLen, byte[] ciphertext, int ciphertextOffset, boolean needsReinit) throws ICCException; */
jint JNICALL
Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_encryptUpdate(J9VMThread *currentThread, jlong iccContextId, jlong iccCipherId, jobject plaintext, jint plaintextOffset, jint plaintextLen, jobject ciphertext, jint ciphertextOffset, jboolean needsReinit)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** CIPHER_encryptUpdate operation\n");
#endif /* CRYPTO_DEBUG */
	bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniCryptoFunctions);
	if (!cryptoLibLoaded) {
		loadCryptoLib(currentThread);
	}
	cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniCryptoFunctions;
	return AES(currentThread, iccContextId, iccCipherId, plaintext, plaintextOffset, plaintextLen, ciphertext, ciphertextOffset, needsReinit, cryptFuncs->CIPHER_encryptUpdate, cryptFuncs->CIPHER_encryptUpdatePtr, 0, 0);
}

/* com.ibm.crypto.plus.provider.icc: static public native int CIPHER_decryptUpdate(long iccContextId, long iccCipherId, byte[] ciphertext, int cipherOffset, int cipherLen, byte[] plaintext, int plaintextOffset, boolean needsReinit) throws ICCException; */
jint JNICALL
Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_decryptUpdate(J9VMThread *currentThread, jlong iccContextId, jlong iccCipherId, jobject ciphertext, jint ciphertextOffset, jint ciphertextLen, jobject plaintext, jint plaintextOffset, jboolean needsReinit)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** CIPHER_decryptUpdate operation\n");
#endif /* CRYPTO_DEBUG */
	bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniCryptoFunctions);
	if (!cryptoLibLoaded) {
		loadCryptoLib(currentThread);
	}
	cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniCryptoFunctions;
	return AES(currentThread, iccContextId, iccCipherId, ciphertext, ciphertextOffset, ciphertextLen, plaintext, plaintextOffset, needsReinit, cryptFuncs->CIPHER_decryptUpdate, cryptFuncs->CIPHER_decryptUpdatePtr, 0, 0);
}

/* com.ibm.crypto.plus.provider.icc: static public native int z_kmc_native (byte[] input, int inputOffset, byte[] output, int outputOffset, long paramPointer, int inputLength, int mode); */
jint JNICALL
Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_z_kmc_native(J9VMThread *currentThread, jobject input, jint inputOffset, jobject output, jint outputOffset, jlong paramPointer, jint inputLength, jint mode)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** z_kmc_native operation\n");
#endif /* CRYPTO_DEBUG */
	bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniCryptoFunctions);
	if (!cryptoLibLoaded) {
		loadCryptoLib(currentThread);
	}
	cryptoPointers_t* cryptFuncs = (cryptoPointers_t*)currentThread->javaVM->jniCryptoFunctions;
	return AES(currentThread, 0, 0, input, inputOffset, inputLength, output, outputOffset, JNI_FALSE, NULL, cryptFuncs->z_kmc_nativePtr, paramPointer, mode);
}

/* com.ibm.crypto.plus.provider.icc: static public native int HMAC_update(long iccContextId, long hmacId, byte[] key, int keyLength, byte[] input, int inputOffset, int inputLength, boolean needInit); */ 
jint JNICALL
Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_HMAC_update(J9VMThread *currentThread, jlong iccContextId, jlong hmacId, jobject key, jint keyLength, jobject input, jint inputOffset, jint inputLength, jboolean needInit)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** HMAC_update operation\n");
#endif /* CRYPTO_DEBUG */
	bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniCryptoFunctions);
	if (!cryptoLibLoaded) {
		loadCryptoLib(currentThread);
	}
	return hmacUpdate(currentThread, iccContextId, hmacId, key, keyLength, input, inputOffset, inputLength, needInit);
}

/* com.ibm.crypto.plus.provider.icc: static public native int HMAC_doFinal(long iccContextId, long hmacId, byte[] key, int keyLength, byte[] hmac, boolean needInit) */ 
jint JNICALL
Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_HMAC_doFinal(J9VMThread *currentThread, jlong iccContextId, jlong hmacId, jobject key, jint keyLength, jobject hmac, jboolean needInit)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** HMAC_doFinal operation\n");
#endif /* CRYPTO_DEBUG */
	bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniCryptoFunctions);
	if (!cryptoLibLoaded) {
		loadCryptoLib(currentThread);
	}
	return hmacFinal(currentThread, iccContextId, hmacId, key, keyLength, hmac, needInit);
}

/* com.ibm.crypto.plus.provider.icc: static public native int DIGEST_update(long iccContextId, long digestId, byte[] input, int offset, int length) */
jint JNICALL 
Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_DIGEST_update(J9VMThread *currentThread, jlong iccContextId, jlong digestId, jobject input, jint offset, jint length)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** DIGEST_update operation\n");
#endif /* CRYPTO_DEBUG */
	bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniCryptoFunctions);
	if (!cryptoLibLoaded) {
		loadCryptoLib(currentThread);
	}
	return digestUpdate(currentThread, iccContextId, digestId, input, offset, length); 
}

/* com.ibm.crypto.plus.provider.icc: static public native int DIGEST_digest_and_reset(long iccContextId, long digestId, byte[] output) */
jint JNICALL 
Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_DIGEST_digest(J9VMThread *currentThread, jlong iccContextId, jlong digestId, jobject digest)
{
#if defined(CRYPTO_DEBUG)
	fprintf(stderr,"** DIGEST_digest operation\n");
#endif /* CRYPTO_DEBUG */
	bool cryptoLibLoaded = (NULL != currentThread->javaVM->jniCryptoFunctions);
	if (!cryptoLibLoaded) {
		loadCryptoLib(currentThread);
	}
	return digestDigest(currentThread, iccContextId, digestId, digest);
}

J9_FAST_JNI_METHOD_TABLE(com_ibm_crypto_plus_provider_icc_NativeInterface)
#if !(defined(WIN32) || defined(WIN64))
	J9_FAST_JNI_METHOD("CIPHER_decryptUpdate", "(JJ[BII[BIZ)I", Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_decryptUpdate,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("CIPHER_encryptUpdate", "(JJ[BII[BIZ)I", Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_encryptUpdate,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("CIPHER_encryptFinal", "(JJ[BII[BIZ)I", Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_encryptFinal,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("CIPHER_decryptFinal", "(JJ[BII[BIZ)I", Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_CIPHER_decryptFinal,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
#endif /* !(defined(WIN32) || defined(WIN64)) */
	J9_FAST_JNI_METHOD("z_kmc_native", "([BI[BIJII)I", Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_z_kmc_native,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("HMAC_update", "(JJ[BI[BIIZ)I", Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_HMAC_update,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("HMAC_doFinal", "(JJ[BI[BZ)I", Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_HMAC_doFinal,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("DIGEST_update", "(JJ[BII)I", Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_DIGEST_update,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("DIGEST_digest_and_reset", "(JJ[B)I", Fast_com_ibm_crypto_plus_provider_icc_NativeInterface_DIGEST_digest,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
J9_FAST_JNI_METHOD_TABLE_END
}
