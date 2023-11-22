/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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

#include "j9.h"
#include "j9protos.h"
#include "j9vmnls.h"
#include "objhelp.h"
#include "ut_j9vm.h"
#include "vm_internal.h"
#include "AtomicSupport.hpp"
#include "ObjectAllocationAPI.hpp"
#include "VMAccess.hpp"

extern "C" {

#if JAVA_SPEC_VERSION >= 16

extern void c_cInterpreter(J9VMThread *currentThread);
extern bool buildCallInStackFrameHelper(J9VMThread *currentThread, J9VMEntryLocalStorage *newELS, bool returnsObject);
extern void restoreCallInFrameHelper(J9VMThread *currentThread);
extern void longJumpWrapperForUpcall(J9VMThread *downCallThread);

static U_8 getInternalTypeFromSignature(J9JavaVM *vm, J9Class *typeClass, U_8 sigType);
static U_64 JNICALL native2InterpJavaUpcallImpl(J9UpcallMetaData *data, void *argsListPointer);
static J9VMThread * getCurrentThread(J9UpcallMetaData *data, bool *isCurThrdAllocated);
static void convertUpcallReturnValue(J9UpcallMetaData *data, U_8 returnType, U_64 *returnStorage);
static bool storeMemArgObjectsToJavaArray(J9UpcallMetaData *data, void *argsListPointer, J9VMThread *currentThread);
static j9object_t createMemSegmentObject(J9UpcallMetaData *data, I_64 offset, I_64 sigTypeSize);
#if JAVA_SPEC_VERSION == 17
static j9object_t createMemAddressObject(J9UpcallMetaData *data, I_64 offset);
static I_64 getNativeAddrFromMemAddressObject(J9UpcallMetaData *data, j9object_t memAddrObject);
#endif /* JAVA_SPEC_VERSION == 17 */
static I_64 getNativeAddrFromMemSegmentObject(J9UpcallMetaData *data, j9object_t memAddrObject);

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and ignore the return value in the case of void.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return void
 */
void JNICALL
native2InterpJavaUpcall0(J9UpcallMetaData *data, void *argsListPointer)
{
	native2InterpJavaUpcallImpl(data, argsListPointer);
}

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return an I_32 value in the case of byte/char/short/int.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return an I_32 value
 */
I_32 JNICALL
native2InterpJavaUpcall1(J9UpcallMetaData *data, void *argsListPointer)
{
	U_64 returnValue = native2InterpJavaUpcallImpl(data, argsListPointer);
	return (I_32)(I_64)returnValue;
}

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return an I_64 value in the case of long/pointer.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return an I_64 value
 */
I_64 JNICALL
native2InterpJavaUpcallJ(J9UpcallMetaData *data, void *argsListPointer)
{
	U_64 returnValue = native2InterpJavaUpcallImpl(data, argsListPointer);
	return (I_64)returnValue;
}

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return a float value as specified in the return type.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return a float
 */
float JNICALL
native2InterpJavaUpcallF(J9UpcallMetaData *data, void *argsListPointer)
{
	U_32 returnValue = (U_32)native2InterpJavaUpcallImpl(data, argsListPointer);
	/* The value returned from the upcall method is literally the single precision (32-bit) IEEE 754 floating-point
	 * representation which must be converted to a real float value before returning back to the native
	 * function in the downcall.
	 */
	return reinterpret_cast<float &>(returnValue);
}

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return a double value as specified in the return type.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return a double
 */
double JNICALL
native2InterpJavaUpcallD(J9UpcallMetaData *data, void *argsListPointer)
{
	U_64 returnValue = native2InterpJavaUpcallImpl(data, argsListPointer);
	/* The value returned from the upcall method is literally the double precision (64-bit) IEEE 754 floating-point
	 * representation which must be converted to a real double value before returning back to the native
	 * function in the downcall.
	 */
	return reinterpret_cast<double &>(returnValue);
}

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return a U_8 pointer to the requested struct.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return a U_8 pointer
 */
U_8 * JNICALL
native2InterpJavaUpcallStruct(J9UpcallMetaData *data, void *argsListPointer)
{
	U_64 returnValue = native2InterpJavaUpcallImpl(data, argsListPointer);
	return (U_8 *)(UDATA)returnValue;
}

/**
 * @brief Determine the predefined return type against the return signature type
 * stored in the native signature array of the upcall metadata.
 *
 * @param data a pointer to J9UpcallMetaData
 * @return a U_8 value for the return type
 */
static U_8
getReturnTypeFromMetaData(J9UpcallMetaData *data)
{
	J9JavaVM *vm = data->vm;
	J9VMThread *currentThread = currentVMThread(vm);
	j9object_t methodType = J9VMJAVALANGINVOKEMETHODHANDLE_TYPE(
			currentThread,
			J9VMOPENJ9INTERNALFOREIGNABIUPCALLMHMETADATA_CALLEEMH(
					currentThread,
					J9_JNI_UNWRAP_REFERENCE(data->mhMetaData)));
	J9Class *retTypeClass = J9VM_J9CLASS_FROM_HEAPCLASS(
			currentThread,
			J9VMJAVALANGINVOKEMETHODTYPE_RTYPE(currentThread, methodType));
	J9UpcallNativeSignature *nativeSig = data->nativeFuncSignature;
	J9UpcallSigType *sigArray = nativeSig->sigArray;
	/* The last element is for the return type. */
	U_8 retSigType = sigArray[nativeSig->numSigs - 1].type & J9_FFI_UPCALL_SIG_TYPE_MASK;
	U_8 retType = getInternalTypeFromSignature(vm, retTypeClass, retSigType);

	return retType;
}

/**
 * @brief Determine the predefined type against the signature type.
 *
 * @param vm a pointer to J9JavaVM
 * @param typeClass a pointer to J9Class
 * @param sigType the requested signature type
 * @return a U_8 value for the predefined type
 */
static U_8
getInternalTypeFromSignature(J9JavaVM *vm, J9Class *typeClass, U_8 sigType)
{
	U_8 dataType = 0;

	switch (sigType) {
	case J9_FFI_UPCALL_SIG_TYPE_VOID:
		dataType = J9NtcVoid;
		break;
	case J9_FFI_UPCALL_SIG_TYPE_CHAR:
		dataType = (typeClass == vm->booleanReflectClass) ? J9NtcBoolean : J9NtcByte;
		break;
	case J9_FFI_UPCALL_SIG_TYPE_SHORT:
		dataType = (typeClass == vm->charReflectClass) ? J9NtcChar : J9NtcShort;
		break;
	case J9_FFI_UPCALL_SIG_TYPE_INT32:
		dataType = J9NtcInt;
		break;
	case J9_FFI_UPCALL_SIG_TYPE_INT64:
		dataType = J9NtcLong;
		break;
	case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
		dataType = J9NtcFloat;
		break;
	case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
		dataType = J9NtcDouble;
		break;
	case J9_FFI_UPCALL_SIG_TYPE_POINTER:
		dataType = J9NtcPointer;
		break;
	case J9_FFI_UPCALL_SIG_TYPE_STRUCT:
		dataType = J9NtcStruct;
		break;
	default:
		Assert_VM_unreachable();
		break;
	}

	return dataType;
}

/**
 * @brief Normalize the argument value based on the predefined signature type.
 *
 * @param argType the predefined signature type
 * @param argValue the requested argument value
 * @return a I_32 value for the normalized argument
 *
 * Note:
 * The argument normalization is only required for types less than 4 bytes
 * in java, which include boolean (1 byte), byte, char (2 bytes), short.
 */
static I_32
getNormalizedArgValue(U_8 argType, I_32 argValue)
{
	I_32 realValue = argValue;

	switch (argType) {
	case J9NtcBoolean:
		realValue = J9_ARE_ANY_BITS_SET(realValue, 0xFF) ? 0x1 : 0x0;
		break;
	case J9NtcByte:
	{
		/* Sign extend to 32 bits ensure the negative value is accessed correctly in upcall. */
		I_8 byteValue = (I_8)realValue;
		realValue = byteValue;
		break;
	}
	case J9NtcChar:
		realValue &= 0xFFFF;
		break;
	case J9NtcShort:
	{
		/* Sign extend to 32 bits ensure the negative value is accessed correctly in upcall. */
		I_16 shortValue = (I_16)realValue;
		realValue = shortValue;
		break;
	}
	default:
		/* Do nothing for the int/float type. */
		break;
	}

	return realValue;
}

/**
 * @brief The common helper function that calls into the interpreter
 * via native2InterpJavaUpcallImpl to invoke the OpenJDK MH in the upcall.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return the expected value against the specified return type
 */
static U_64 JNICALL
native2InterpJavaUpcallImpl(J9UpcallMetaData *data, void *argsListPointer)
{
	J9VMThread *downCallThread = data->downCallThread;
	J9UpcallNativeSignature *nativeSig = data->nativeFuncSignature;
	J9UpcallSigType *sigArray = nativeSig->sigArray;
	I_32 paramCount = (I_32)(nativeSig->numSigs - 1); /* The last element is for the return type. */
	U_8 returnType = 0;
	bool returnsObject = false;
	J9VMEntryLocalStorage newELS = {0};
	J9VMThread *currentThread = NULL;
	bool isCurThrdAllocated = false;
	U_64 returnStorage = 0;

#if JAVA_SPEC_VERSION >= 21
		/* Capture the invalid linker option (intended for the trivial downcall as specified in JDK21) in upcall
		 * by throwing out an exception so as to remind users of the incorrect behavior in applications rather
		 * than ending up with an assertion failure by crashing the JVM in the RI implementation.
		 */
		if (downCallThread->isInCriticalDownCall) {
			setCurrentExceptionNLS(downCallThread, J9VMCONSTANTPOOL_JAVALANGILLEGALTHREADSTATEEXCEPTION, J9NLS_VM_ILLEGAL_THREAD_STATE_UPCALL);
			goto illegalState;
		}
#endif /* JAVA_SPEC_VERSION => 21 */

	/* Determine whether to use the current thread or create a new one
	 * when there is no java thread attached to the native thread
	 * created directly in native.
	 */
	currentThread = getCurrentThread(data, &isCurThrdAllocated);
	if (NULL == currentThread) {
		/* The OOM exception set in getCurrentThread() will be thrown in the interpreter
		 * after returning from the native function in downcall.
		 */
		goto doneAndExit;
	}

	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	returnType = getReturnTypeFromMetaData(data);
	returnsObject = (J9NtcPointer == returnType) || (J9NtcStruct == returnType);

	if (buildCallInStackFrameHelper(currentThread, &newELS, returnsObject)) {
		j9object_t mhMetaData = NULL;
		j9object_t upcallMH = NULL;
		j9object_t nativeArgArray = NULL;
		j9object_t argTypes = NULL;

		/* Store the allocated memory objects for the struct/pointer arguments to the java array. */
		if (!storeMemArgObjectsToJavaArray(data, argsListPointer, currentThread)) {
			goto done;
		}

		mhMetaData = J9_JNI_UNWRAP_REFERENCE(data->mhMetaData);
		upcallMH = J9VMOPENJ9INTERNALFOREIGNABIUPCALLMHMETADATA_CALLEEMH(currentThread, mhMetaData);
		nativeArgArray = J9VMOPENJ9INTERNALFOREIGNABIUPCALLMHMETADATA_NATIVEARGARRAY(currentThread, mhMetaData);
		argTypes = J9VMJAVALANGINVOKEMETHODTYPE_PTYPES(currentThread, J9VMJAVALANGINVOKEMETHODHANDLE_TYPE(currentThread, upcallMH));

		/* The argument list of the upcall method handle on the stack includes the target method handle,
		 * the method arguments and the appendix which is set via MethodHandleResolver.ffiCallLinkCallerMethod().
		 */
		*(j9object_t*)--(currentThread->sp) = upcallMH;

		for (I_32 argIndex = 0; argIndex < paramCount; argIndex++) {
			U_8 argSigType = sigArray[argIndex].type & J9_FFI_UPCALL_SIG_TYPE_MASK;
			j9object_t argTypeObject = J9JAVAARRAYOFOBJECT_LOAD(currentThread, argTypes, argIndex);
			J9Class *argTypeClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, argTypeObject);
			U_8 argType = getInternalTypeFromSignature(data->vm, argTypeClass, argSigType);

			switch (argSigType) {
			/* Small native types are saved at their natural boundaries (1, 2, 4-byte) in the stack on macOS/AArch64
			 * while the stack slots are always 8-byte wide on Linux/AArch64.
			 */
#if defined(OSX) && defined(AARCH64)
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:
			{
				I_8 argValue = *(I_8*)getArgPointer(nativeSig, argsListPointer, argIndex);
				*(I_32*)--(currentThread->sp) = getNormalizedArgValue(argType, (I_32)argValue);
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:
			{
				I_16 argValue = *(I_16*)getArgPointer(nativeSig, argsListPointer, argIndex);
				*(I_32*)--(currentThread->sp) = getNormalizedArgValue(argType, (I_32)argValue);
				break;
			}
#else
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT: /* Fall through */
#endif /* defined(OSX) && defined(AARCH64) */
			case J9_FFI_UPCALL_SIG_TYPE_INT32: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
			{
				/* Convert the argument value to 64 bits prior to the 32-bit conversion to get the actual value
				 * in the case of boolean/byte/char/short/int regardless of the endianness on platforms.
				 */
				I_64 argValue = *(I_64*)getArgPointer(nativeSig, argsListPointer, argIndex);
#if !defined(J9VM_ENV_LITTLE_ENDIAN)
				/* Right shift the 64-bit float argument by 4 bytes (32 bits) given the actual value
				 * is placed on the higher 4 bytes on the Big-Endian (BE) platforms.
				 */
				if (J9_FFI_UPCALL_SIG_TYPE_FLOAT == argSigType) {
					argValue = argValue >> J9_FFI_UPCALL_SIG_TYPE_32_BIT;
				}
#endif /* !defined(J9VM_ENV_LITTLE_ENDIAN) */
				*(I_32*)--(currentThread->sp) = getNormalizedArgValue(argType, (I_32)argValue);
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_INT64: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
				currentThread->sp -= 2;
				*(I_64*)(currentThread->sp) = *(I_64*)getArgPointer(nativeSig, argsListPointer, argIndex);
				break;
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT:
				*(j9object_t*)--(currentThread->sp) = J9JAVAARRAYOFOBJECT_LOAD(currentThread, nativeArgArray, argIndex);
				break;
			default:
				Assert_VM_unreachable();
				break;
			}
		}

		/* Place mhMetaData as the return value to native2InterpreterTransition() when calling into
		 * the interpreter so as to set the invoke cache array (MemberName and appendix)
		 * before invoking the target handle in upcall.
		 */
		currentThread->returnValue = J9_BCLOOP_N2I_TRANSITION;
		currentThread->returnValue2 = (UDATA)data;
		c_cInterpreter(currentThread);

done:
		restoreCallInFrameHelper(currentThread);
	}

	/* Transfer the exception from the locally created upcall thread to the downcall thread
	 * as the upcall thread will be cleaned up before the dispatcher exits; otherwise
	 * the current thread's exception can be brought back to the interpreter in downcall.
	 *
	 * Note:
	 * The exception could be OOM which is set for the downcall thread
	 * in storeMemArgObjectsToJavaArray().
	 */
	if (!VM_VMHelpers::exceptionPending(downCallThread)) {
		if (VM_VMHelpers::exceptionPending(currentThread)) {
			if (isCurThrdAllocated) {
				downCallThread->currentException = currentThread->currentException;
				currentThread->currentException = NULL;
			}
		} else {
			/* Read returnStorage from returnValue (and returnValue2 on 32-bit platforms). */
			returnStorage = *(U_64 *)&currentThread->returnValue;
			convertUpcallReturnValue(data, returnType, &returnStorage);
		}
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);

	/* Release the locally created thread given the underlying thread created in the native function
	 * will be destroyed soon once the dispatcher exits and returns to the interpreter.
	 *
	 * Note:
	 * we can't rely on J9_PRIVATE_FLAGS_FFI_UPCALL_THREAD as the currently created thread might
	 * recursively trigger the downcall handler in the interpreter and subsequently dispatcher
	 * in upcall, in which case only the first dispatcher that creates the current thread is able
	 * to clean it up.
	 */
	if (isCurThrdAllocated) {
		threadCleanup(currentThread, false);
		currentThread = NULL;
	}

#if JAVA_SPEC_VERSION >= 21
illegalState:
#endif /* JAVA_SPEC_VERSION => 21 */
	/* Restore back to the setjump site in the call-out native
	 * to handle the captured exception.
	 *
	 * See inlInternalDowncallHandlerInvokeNative()
	 * in BytecodeInterpreter.hpp for details.
	 */
	if (VM_VMHelpers::exceptionPending(downCallThread)) {
		longJumpWrapperForUpcall(downCallThread);
	}

doneAndExit:
	return returnStorage;
}

/**
 * @brief Get a J9VMThread whether it is the current thread or a newly created thread if doesn't exist.
 *
 * Note:
 * The function is to handle the situation when there is no Java thread for the current native thread
 * which is directly created in native code without a Java thread attached to it.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param isCurThrdAllocated a pointer to a flag indicating whether the current thread is allocated locally
 * @return a pointer to J9VMThread
 */
static J9VMThread *
getCurrentThread(J9UpcallMetaData *data, bool *isCurThrdAllocated)
{
	J9JavaVM *vm = data->vm;
	J9VMThread *downCallThread = data->downCallThread;
	J9VMThread *currentThread = currentVMThread(vm);
	omrthread_t osThread = NULL;

	if (NULL == currentThread) {
		/* Attach to the created OMR thread after setting the omr thread attributes. */
		if (J9THREAD_SUCCESS != attachThreadWithCategory(&osThread, J9THREAD_CATEGORY_APPLICATION_THREAD)) {
			goto oom;
		}

		/* Attach to the created J9VMThread after finishing all required steps which include the setting
		 * of the threadName & threadObject, and triggering the jvmtihook start event for the thread, etc.
		 */
		if (JNI_OK != internalAttachCurrentThread(vm, &currentThread, NULL,
				J9_PRIVATE_FLAGS_ATTACHED_THREAD | J9_PRIVATE_FLAGS_FFI_UPCALL_THREAD, osThread)) {
			omrthread_detach(osThread);
			osThread = NULL;
			goto oom;
		}
		*isCurThrdAllocated = true;
	}

done:
	return currentThread;

oom:
	/* The exception will be thrown from the downcall thread in inlInternalDowncallHandlerInvokeNative()
	 * after returning back to the interpreter.
	 */
	setNativeOutOfMemoryError(downCallThread, 0, 0);
	goto done;

}

/**
 * @brief Converts the type of the return value to the return type intended for upcall.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param returnType the type for the return value
 * @param returnStorage a pointer to the return value
 *
 * Note:
 * The VMAccess is required for the caller to do the exception check on struct/pointer.
 */
static void
convertUpcallReturnValue(J9UpcallMetaData *data, U_8 returnType, U_64 *returnStorage)
{
	switch (returnType) {
	case J9NtcBoolean: /* Fall through */
	case J9NtcByte:    /* Fall through */
	case J9NtcChar:    /* Fall through */
	case J9NtcShort:   /* Fall through */
	case J9NtcInt:     /* Fall through */
	case J9NtcFloat:
	{
#if !defined(J9VM_ENV_LITTLE_ENDIAN)
		/* Right shift the returned value from the upcall method by 4 bytes (32 bits) for the signature type
		 * less than or equal to 4 bytes in size given the actual value is placed on the higher 4 bytes
		 * on the Big-Endian (BE) platforms.
		 */
		*returnStorage = *returnStorage >> J9_FFI_UPCALL_SIG_TYPE_32_BIT;
#endif /* !defined(J9VM_ENV_LITTLE_ENDIAN) */
		break;
	}
#if JAVA_SPEC_VERSION == 17
	case J9NtcPointer:
	{
		j9object_t memAddrObject = (j9object_t)*returnStorage;
		*returnStorage = (U_64)getNativeAddrFromMemAddressObject(data, memAddrObject);
		break;
	}
#else /* JAVA_SPEC_VERSION == 17 */
	case J9NtcPointer: /* Fall through */
#endif /* JAVA_SPEC_VERSION == 17 */
	case J9NtcStruct:
	{
		j9object_t memSegmtObject = (j9object_t)*returnStorage;
		*returnStorage = (U_64)getNativeAddrFromMemSegmentObject(data, memSegmtObject);
		break;
	}
	default:
		/* Nothing is required for void/long/double upon return. */
		break;
	}
}

/**
 * @brief Allocate memory related objects for the struct/pointer arguments
 * to store them to the java array before placing them on the java stack
 * for upcall.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @param currentThread a pointer to J9VMThread
 * @return true if the storing operation is completed; otherwise return false;
 */
static bool
storeMemArgObjectsToJavaArray(J9UpcallMetaData *data, void *argsListPointer, J9VMThread *currentThread)
{
	J9UpcallNativeSignature *nativeSig = data->nativeFuncSignature;
	J9UpcallSigType *sigArray = nativeSig->sigArray;
	I_32 paramCount = (I_32)(nativeSig->numSigs - 1); /* The last element is for the return type. */
	bool result = true;

	for (I_32 argIndex = 0; argIndex < paramCount; argIndex++) {
		U_8 argSigType = sigArray[argIndex].type & J9_FFI_UPCALL_SIG_TYPE_MASK;
		j9object_t memArgObject = NULL;
		j9object_t nativeArgArray = NULL;

		if ((J9_FFI_UPCALL_SIG_TYPE_POINTER == argSigType)
			|| (J9_FFI_UPCALL_SIG_TYPE_STRUCT == argSigType)
		) {
			if (J9_FFI_UPCALL_SIG_TYPE_POINTER == argSigType) {
				I_64 offset = *(I_64*)getArgPointer(nativeSig, argsListPointer, argIndex);
#if JAVA_SPEC_VERSION >= 20
				/* A pointer argument is wrapped as an unbounded memory segment in upcall
				 * to pass the access check on the boundary as specified in JDK20.
				 */
				memArgObject = createMemSegmentObject(data, offset, LONG_MAX);
#else /* JAVA_SPEC_VERSION => 20 */
				memArgObject = createMemAddressObject(data, offset);
#endif /* JAVA_SPEC_VERSION => 20 */
			} else { /* J9_FFI_UPCALL_SIG_TYPE_STRUCT */
				I_64 offset = (I_64)(intptr_t)getArgPointer(nativeSig, argsListPointer, argIndex);
				memArgObject = createMemSegmentObject(data, offset, sigArray[argIndex].sizeInByte);
			}

			if (NULL == memArgObject) {
				/* The OOM exception set in createMemAddressObject/createMemSegmentObject will be
				 * thrown in the interpreter after returning from the native function in downcall.
				 */
				result = false;
				goto done;
			}
		}

		/* Store the struct/pointer object (or null in the case of the primitive types) in the
		 * java array so as to avoid being updated by GC (which is triggered by J9AllocateObject
		 * in createMemAddressObject/createMemSegmentObject) when allocating memory for the next
		 * struct/pointer of the argument list.
		 */
		nativeArgArray = J9VMOPENJ9INTERNALFOREIGNABIUPCALLMHMETADATA_NATIVEARGARRAY(
				currentThread,
				J9_JNI_UNWRAP_REFERENCE(data->mhMetaData));
		J9JAVAARRAYOFOBJECT_STORE(currentThread, nativeArgArray, argIndex, memArgObject);
	}

done:
	return result;
}

#if JAVA_SPEC_VERSION == 17
/**
 * @brief Generate an object of the MemoryAddress's subclass on the heap
 * with the specified native address to the value.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param offset the native address to the value
 * @return a MemoryAddress object
 */
static j9object_t
createMemAddressObject(J9UpcallMetaData *data, I_64 offset)
{
	J9JavaVM * vm = data->vm;
	J9VMThread *downCallThread = data->downCallThread;
	J9VMThread *currentThread = currentVMThread(vm);
	MM_ObjectAllocationAPI objectAllocate(currentThread);
	J9Class *memAddrClass = J9VMJDKINTERNALFOREIGNMEMORYADDRESSIMPL(vm);
	j9object_t memAddrObject = NULL;

	/* To wrap up an object of the MemoryAddress's subclass as an argument on the java stack,
	 * this object is directly allocated on the heap with the passed-in native address (offset)
	 * set to this object.
	 */
	memAddrObject = objectAllocate.inlineAllocateObject(currentThread, memAddrClass, true, false);
	if (NULL == memAddrObject) {
		memAddrObject = vm->memoryManagerFunctions->J9AllocateObject(currentThread, memAddrClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
		if (NULL == memAddrObject) {
			/* Directly set the OOM error to the downcall thread to bring it up back to the interpreter
			 * in downcall when the upcall thread is the same as the downcall thread; otherwise, the OOM
			 * error should be still set to the downcall thread given the locally created native thread
			 * in upcall will be cleaned up before the dispatcher exists and returns to the interpreter.
			 */
			setHeapOutOfMemoryError(downCallThread);
			goto done;
		}
	}

	J9VMJDKINTERNALFOREIGNMEMORYADDRESSIMPL_SET_SEGMENT(currentThread, memAddrObject, NULL);
	J9VMJDKINTERNALFOREIGNMEMORYADDRESSIMPL_SET_OFFSET(currentThread, memAddrObject, offset);

done:
	return memAddrObject;
}
#endif /* JAVA_SPEC_VERSION == 17 */

/**
 * @brief Generate an object of the MemorySegment's subclass on the heap with the specified
 * native address to the requested struct or the unbounded pointer (JDK20+).
 *
 * @param data a pointer to J9UpcallMetaData
 * @param offset the native address to the requested struct or the unbounded pointer (JDK20+)
 * @param sigTypeSize the byte size of the requested struct or the unbounded pointer (JDK20+)
 * @return a MemorySegment object
 */
static j9object_t
createMemSegmentObject(J9UpcallMetaData *data, I_64 offset, I_64 sigTypeSize)
{
	J9JavaVM *vm = data->vm;
	J9VMThread *downCallThread = data->downCallThread;
	J9VMThread *currentThread = currentVMThread(vm);
	MM_ObjectAllocationAPI objectAllocate(currentThread);
	j9object_t memSegmtObject = NULL;
	j9object_t mhMetaData = NULL;
	J9Class *memSegmtClass = J9VMJDKINTERNALFOREIGNNATIVEMEMORYSEGMENTIMPL(vm);

	/* To wrap up an object of the MemorySegment's subclass as an argument on the java stack,
	 * this object is directly allocated on the heap with the passed-in native address (offset)
	 * set to this object.
	 */
	memSegmtObject = objectAllocate.inlineAllocateObject(currentThread, memSegmtClass, true, false);
	if (NULL == memSegmtObject) {
		memSegmtObject = vm->memoryManagerFunctions->J9AllocateObject(currentThread, memSegmtClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
		if (NULL == memSegmtObject) {
			/* Directly set the OOM error to the downcall thread to bring it up back to the interpreter
			 * in downcall when the upcall thread is the same as the downcall thread; otherwise, the OOM
			 * error should be still set to the downcall thread given the locally created native thread
			 * in upcall will be cleaned up before the dispatcher exists and returns to the interpreter.
			 */
			setHeapOutOfMemoryError(downCallThread);
			goto done;
		}
	}
	mhMetaData = J9_JNI_UNWRAP_REFERENCE(data->mhMetaData);

	J9VMJDKINTERNALFOREIGNNATIVEMEMORYSEGMENTIMPL_SET_MIN(currentThread, memSegmtObject, offset);
	J9VMJDKINTERNALFOREIGNNATIVEMEMORYSEGMENTIMPL_SET_LENGTH(currentThread, memSegmtObject, sigTypeSize);
	J9VMJDKINTERNALFOREIGNNATIVEMEMORYSEGMENTIMPL_SET_SCOPE(
			currentThread,
			memSegmtObject,
			J9VMOPENJ9INTERNALFOREIGNABIUPCALLMHMETADATA_SCOPE(currentThread, mhMetaData));

done:
	return memSegmtObject;
}

#if JAVA_SPEC_VERSION == 17
/**
 * @brief Get the native address to the requested value from a MemoryAddress object.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param memAddrObject the specified MemoryAddress object
 * @return the native address to the value in the memory
 *
 * Note:
 * There are two cases for the calculation of the native memory address (offset) as follows:
 * 1) if the offset is generated via createMemAddressObject() in native and passed over into java,
 *    then the offset is the requested native address value;
 * 2) MemorySegment.address() is invoked upon return in java, which means:
 *    address = segment.min() as specified in MemoryAddressImpl (offset is set to zero)
 */
static I_64
getNativeAddrFromMemAddressObject(J9UpcallMetaData *data, j9object_t memAddrObject)
{
	J9VMThread *currentThread = currentVMThread(data->vm);
	I_64 offset = J9VMJDKINTERNALFOREIGNMEMORYADDRESSIMPL_OFFSET(currentThread, memAddrObject);
	I_64 nativePtrValue = offset;
	j9object_t segmtObject = J9VMJDKINTERNALFOREIGNMEMORYADDRESSIMPL_SEGMENT(currentThread, memAddrObject);
	/* The offset is set to zero in AbstractMemorySegmentImpl.address() in OpenJDK. */
	if (NULL != segmtObject) {
		nativePtrValue = J9VMJDKINTERNALFOREIGNNATIVEMEMORYSEGMENTIMPL_MIN(currentThread, segmtObject);
	}
	return nativePtrValue;
}
#endif /* JAVA_SPEC_VERSION == 17 */

/**
 * @brief Get the native address to the requested struct from a MemorySegment object.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param memSegmtObject the specified MemorySegment object
 * @return the native address to the requested struct
 */
static I_64
getNativeAddrFromMemSegmentObject(J9UpcallMetaData *data, j9object_t memSegmtObject)
{
	J9VMThread *currentThread = currentVMThread(data->vm);
	return (I_64)J9VMJDKINTERNALFOREIGNNATIVEMEMORYSEGMENTIMPL_MIN(currentThread, memSegmtObject);
}
#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
