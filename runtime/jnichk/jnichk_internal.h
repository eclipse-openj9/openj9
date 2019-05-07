/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#ifndef jnichk_internal_h
#define jnichk_internal_h

/**
* @file jnichk_internal.h
* @brief Internal prototypes used within the JNICHK module.
*
* This file contains implementation-private function prototypes and
* type definitions for the JNICHK module.
*
*/

#include "j9.h"
#include "jni.h"
#include "j9comp.h"
#include "j9protos.h"
#include "jnichk_api.h"
#include <string.h>
#include <stdlib.h>

#define CRITICAL_UNSAFE 0 /* it is unsafe to call from within a critical region (see getPrimitiveArrayCritical and getStringCritical) */
#define CRITICAL_SAFE 1 /* it is safe to call from within a critical region */
#define CRITICAL_WARN 2 /* works on J9, but is counter to the JNI specification - warn when in pedantic mode */

#define METHOD_INSTANCE 0
#define METHOD_STATIC 1
#define METHOD_CONSTRUCTOR 2

#define JNIC_MASK 0x000000FF

/* this first group of identifying characters must be consistent with the Java VM spec. */
#define JNIC_JBYTE 'B'
#define JNIC_JBOOLEAN 'Z'
#define JNIC_JCHAR 'C'
#define JNIC_JSHORT 'S'
#define JNIC_JINT 'I'
#define JNIC_JLONG 'J'
#define JNIC_JFLOAT 'F'
#define JNIC_JDOUBLE 'D'
#define JNIC_JOBJECT 'L'
#define JNIC_VOID 'V'

#define JNIC_JMETHODID 'm'
#define JNIC_JFIELDID 'n'
#define JNIC_VALIST 'v'
#define JNIC_JSIZE 'q'
#define JNIC_POINTER 'p'
#define JNIC_STRING '$'
#define JNIC_CLASSNAME '@'
#define JNIC_MEMBERNAME '#'
#define JNIC_METHODSIGNATURE '!'
#define JNIC_FIELDSIGNATURE '%'
#define JNIC_JOBJECTARRAY 'l'
#define JNIC_JARRAY 'a'
#define JNIC_JINTARRAY 'i'
#define JNIC_JDOUBLEARRAY 'd'
#define JNIC_JCHARARRAY 'c'
#define JNIC_JSHORTARRAY 's'
#define JNIC_JBYTEARRAY 'b'
#define JNIC_JBOOLEANARRAY 'z'
#define JNIC_JFLOATARRAY 'f'
#define JNIC_JLONGARRAY 'j'
#define JNIC_JCLASS 'K'
#define JNIC_JSTRING 'G'
#define JNIC_JTHROWABLE 'T'
#define JNIC_DIRECTBUFFER '~'
#define JNIC_REFLECTMETHOD 'M'
#define JNIC_REFLECTFIELD 'N'
#define JNIC_NONNULLOBJECT '0'
#define JNIC_WEAKREF 'w'
#define JNIC_GLOBALREF '*'
#define JNIC_LOCALREF '?'		

/* Global Ref tracking hash table entry */
typedef struct JNICHK_GREF_HASHENTRY {
	UDATA reference;
	BOOLEAN alive;
} JNICHK_GREF_HASHENTRY;		


#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)

#define enterVM(currentThread) \
	BOOLEAN hasNoVMAccess = J9_ARE_NO_BITS_SET((currentThread)->publicFlags, J9_PUBLIC_FLAGS_VM_ACCESS); \
	UDATA inNative = (currentThread)->inNative; \
	do { \
		if (inNative) { \
			enterVMFromJNI(currentThread); \
		} else { \
			if (hasNoVMAccess) { \
				acquireVMAccess(currentThread); \
			} \
		} \
	} while(0)
		
#define exitVM(currentThread) \
	do { \
		if (inNative) { \
			exitVMToJNI(currentThread); \
		} else if (hasNoVMAccess) { \
			releaseVMAccess(currentThread); \
		} \
	} while(0)

#else /* J9VM_INTERP_ATOMIC_FREE_JNI */

#define enterVM(currentThread) \
	BOOLEAN hasNoVMAccess = J9_ARE_NO_BITS_SET((currentThread)->publicFlags, J9_PUBLIC_FLAGS_VM_ACCESS); \
	do { \
		if (hasNoVMAccess) { \
			acquireVMAccess(currentThread); \
		} \
	} while(0)

#define exitVM(currentThread) \
	do { \
		if (hasNoVMAccess) { \
			releaseVMAccess(currentThread); \
		} \
	} while(0)

#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- jnicheck.c ---------------- */

/**
* @brief
* @param env
* @param vaptr
* @param result
* @return void
*/
void jniCallInReturn_jboolean(JNIEnv* env, void * vaptr, jboolean result);


/**
* @brief
* @param env
* @param vaptr
* @param result
* @return void
*/
void jniCallInReturn_jbyte(JNIEnv* env, void * vaptr, jbyte result);


/**
* @brief
* @param env
* @param vaptr
* @param result
* @return void
*/
void jniCallInReturn_jchar(JNIEnv* env, void * vaptr, jchar result);


/**
* @brief
* @param env
* @param vaptr
* @param result
* @return void
*/
void jniCallInReturn_jdouble(JNIEnv* env, void * vaptr, jdouble result);


/**
* @brief
* @param env
* @param vaptr
* @param result
* @return void
*/
void jniCallInReturn_jfloat(JNIEnv* env, void * vaptr, jfloat result);


/**
* @brief
* @param env
* @param vaptr
* @param result
* @return void
*/
void jniCallInReturn_jint(JNIEnv* env, void * vaptr, jint result);


/**
* @brief
* @param env
* @param vaptr
* @param result
* @return void
*/
void jniCallInReturn_jlong(JNIEnv* env, void * vaptr, jlong result);


/**
* @brief
* @param env
* @param vaptr
* @param result
* @return void
*/
void jniCallInReturn_jobject(JNIEnv* env, void * vaptr, jobject result);


/**
* @brief
* @param env
* @param vaptr
* @param result
* @return void
*/
void jniCallInReturn_jshort(JNIEnv* env, void * vaptr, jshort result);


/**
* @brief
* @param env
* @param vaptr
* @return void
*/
void jniCallInReturn_void(JNIEnv* env, void * vaptr);


/**
* @brief
* @param env
* @param nlsModule
* @param nlsIndex
* @param ...
* @return void
*/
void 
jniCheckAdviceNLS(JNIEnv* env, U_32 nlsModule, U_32 nlsIndex, ...);


/**
* @brief
* @param function
* @param exceptionSafe
* @param criticalSafe
* @param refTracking
* @param descriptor
* @param env
* @param ...
* @return void
*/
void 
jniCheckArgs(const char* function, int exceptionSafe, int criticalSafe, J9JniCheckLocalRefState* refTracking, const U_32* descriptor, JNIEnv* env, ...);


/**
* @brief
* @param env
* @param function
* @param array
* @param start
* @param len
* @return void
*/
void jniCheckArrayRange(JNIEnv* env, const char* function, jarray array, jint start, jsize len);


/**
* @brief
* @param function
* @param env
* @param receiver
* @param methodType
* @param returnType
* @param method
* @param args
* @return void
*/
void 
jniCheckCallA(const char* function, JNIEnv* env, jobject receiver, UDATA methodType, UDATA returnType, jmethodID method, jvalue* args);


/**
* @brief
* @param function
* @param env
* @param receiver
* @param methodType
* @param returnType
* @param method
* @param originalArgs
* @return void
*/
void 
jniCheckCallV(const char* function, JNIEnv* env, jobject receiver, UDATA methodType, UDATA returnType, jmethodID method, va_list originalArgs);


/**
* @brief
* @param env
* @param function
* @param argNum
* @param aJobject
* @param expectedClass
* @param expectedType
* @return void
*/
void jniCheckClass(JNIEnv* env, const char* function, IDATA argNum, jobject aJobject, J9Class* expectedClass, const char* expectedType);


/**
* @brief
* @param env
* @param function
* @param argNum
* @param aJobject
* @return void
*/
void 
jniCheckDirectBuffer(JNIEnv* env, const char* function, IDATA argNum, jobject aJobject);


/**
* @brief
* @param env
* @param nlsModule
* @param nlsIndex
* @param ...
* @return void
*/
void jniCheckFatalErrorNLS(JNIEnv* env, U_32 nlsModule, U_32 nlsIndex, ...);


/**
* @brief
* @param void
* @return const char*
*/
const char*
jniCheckGetPotentialPendingException(void);


/**
* @brief
* @param env
* @param function
* @param argNum
* @param reference
* @return void
*/
void jniCheckGlobalRef(JNIEnv* env,  const char* function, IDATA argNum, jobject reference);


/**
* @brief
* @param env
* @param function
* @param argNum
* @param reference
* @return void
*/
void jniCheckLocalRef(JNIEnv* env,  const char* function, IDATA argNum, jobject reference);


/**
* @brief
* @param env
* @param function
* @param savedState
* @return void
*/
void jniCheckLocalRefTracking(JNIEnv* env, const char* function, J9JniCheckLocalRefState* savedState);


/**
* @brief
* @param env
* @param function
* @param argNum
* @param obj
* @return void
*/
void jniCheckNull(JNIEnv* env, const char* function, IDATA argNum, jobject obj);


/**
* @brief
* @param env
* @param function
* @return void
*/
void
jniCheckPopLocalFrame(JNIEnv* env, const char* function);


/**
* @brief
* @param env
* @param function
* @param type
* @param arg
* @param argNum
* @param min
* @param max
* @return void
*/
void 
jniCheckRange(JNIEnv* env,  const char* function, const char* type, IDATA arg, IDATA argNum, IDATA min, IDATA max);


/**
* @brief
* @param env
* @param function
* @param argNum
* @param reference
* @return void
*/
void 
jniCheckRef(JNIEnv* env,  const char* function, IDATA argNum, jobject reference);


/**
* @brief
* @param env
* @param function
* @param argNum
* @param aJobject
* @return void
*/
void 
jniCheckReflectMethod(JNIEnv* env, const char* function, IDATA argNum, jobject aJobject);


/**
* @brief
* @param function
* @return void
*/
void
jniCheckSetPotentialPendingException(const char* function);


/**
* @brief
* @param env
* @param function
* @param string
* @param start
* @param len
* @return void
*/
void jniCheckStringRange(JNIEnv* env, const char* function, jstring string, jint start, jsize len);


/**
* @brief
* @param env
* @param function
* @param string
* @param start
* @param len
* @return void
*/
void jniCheckStringUTFRange(JNIEnv* env, const char* function, jstring string, jint start, jsize len);


/**
* @brief
* @param env
* @param function
* @param argNum
* @param aJobject
* @param type
* @return void
*/
void jniCheckSubclass(JNIEnv* env, const char* function, IDATA argNum, jobject aJobject, const char* type);


/**
* @brief
* @param env
* @param nlsModule
* @param nlsIndex
* @param ...
* @return void
*/
void 
jniCheckWarningNLS(JNIEnv* env, U_32 nlsModule, U_32 nlsIndex, ...);


/**
* @brief
* @param env
* @param function
* @param argNum
* @param reference
* @return void
*/
void jniCheckWeakGlobalRef(JNIEnv* env,  const char* function, IDATA argNum, jobject reference);


/**
* @brief
* @param function
* @param env
* @param name
* @return void
*/
void jniVerboseFindClass(const char* function, JNIEnv* env, const char* name);


/**
* @brief
* @param function
* @param env
* @param clazz
* @param name
* @param sig
* @return void
*/
void jniVerboseGetID(const char* function, JNIEnv* env, jclass clazz, const char* name, const char* sig);


/* ---------------- jnicmem.c ---------------- */

/**
* @brief
* @param env
* @return void
*/
void 
jniCheckForUnreleasedMemory(JNIEnv* env);


/**
* @brief
* @param vmThread
* @return jint
*/
jint
jniCheckMemoryInit(J9JavaVM* javaVM);


/**
* @brief
* @param env
* @param functionName
* @param object
* @param memory
* @param recordCRC
* @return void
*/
void 
jniRecordMemoryAcquire(JNIEnv* env, const char* functionName, jobject object, const void* memory, jint recordCRC);


/**
* @brief
* @param env
* @param acquireFunction
* @param releaseFunction
* @param object
* @param memory
* @param checkCRC
* @param mode
* @return void
*/
void 
jniRecordMemoryRelease(JNIEnv* env, const char* acquireFunction, const char* releaseFunction, jobject object, const void* memory, jint checkCRC, jint mode);

/**
 * @brief Flush any cached JNI memory if memorycheck is enabled so that it may be validated immediately
 * @param env The current thread
 * @return void
 */
void
jniCheckFlushJNICache(JNIEnv* env);

/* ---------------- jnicbuf.c ---------------- */

/**
 * Compute the CRC of a buffer.
 * The buffer is a NUL-terminated string.
 * If buf is NULL, return 0.
 * @param buf The NUL-terminated string
 * @return U_32 the CRC for the buffer
 * @see checkStringCRC
 */
U_32 computeStringCRC(const char* buf);

/**
 * Check that the specified buffer has not changed since its CRC was initially computed.
 * If it has, issue a -Xcheck:jni error.
 * @param env The JNIEnv pointer for the current thread
 * @param fnName The name of the JNI function being checked
 * @param argNum The index of the buffer argument in the JNI function's argument list
 * @param buf A NUL-terminated string
 * @param oldCRC The CRC value computed by computeStringCRC
 * @return void
 */
void checkStringCRC(JNIEnv* env, const char* fnName, U_32 argNum, const char* buf, U_32 oldCRC);


/**
 * Compute the CRC of a buffer.
 * The buffer is an array of bytes whose length is specified.
 * If buf is NULL or len is negative, return 0.
 * @param buf The data pointer
 * @param len The number of bytes in the buffer
 * @return U_32 the CRC for the buffer
 * @see checkDataCRC
 */
U_32 computeDataCRC(const void* buf, IDATA len);

/**
 * Check that the specified buffer has not changed since its CRC was initially computed.
 * If it has, issue a -Xcheck:jni error.
 * @param env The JNIEnv pointer for the current thread
 * @param fnName The name of the JNI function being checked
 * @param argNum The index of the buffer argument in the JNI function's argument list
 * @param buf The data pointer
 * @param len The number of bytes in the buffer
 * @param oldCRC The CRC value computed by computeDataCRC
 * @return void
 */
void checkDataCRC(JNIEnv* env, const char* fnName, U_32 argNum, const void* buf, IDATA len, U_32 oldCRC);

/**
 * Compute the CRC of a buffer.
 * The buffer is a method argument array of jvalues
 * If buf is NULL, return 0.
 * @param buf The data pointer
 * @param methodID The method associated with the args (used to calculate the length)
 * @return U_32 the CRC for the buffer
 * @see checkArgsCRC
 */
U_32 computeArgsCRC(const jvalue *args, jmethodID methodID);

/**
 * Check that the specified buffer has not changed since its CRC was initially computed.
 * If it has, issue a -Xcheck:jni error.
 * @param env The JNIEnv pointer for the current thread
 * @param fnName The name of the JNI function being checked
 * @param argNum The index of the buffer argument in the JNI function's argument list
 * @param buf The data pointer
 * @param methodID The method associated with the args (used to calculate the length)
 * @param oldCRC The CRC value computed by computeArgsCRC
 * @return void
 */
void checkArgsCRC(JNIEnv* env, const char* fnName, U_32 argNum, const jvalue *args, jmethodID methodID, U_32 oldCRC);

/**
 * Check if the class making the JNI call is in a System library class or not.
 * @param env The JNIEnv pointer for the current thread.
 * @return JNI_TRUE if the class is a bootstrap class, JNI_FALSE otherwise.
 */
jboolean inBootstrapClass (JNIEnv* env);
#ifdef __cplusplus
}
#endif

#endif /* jnichk_internal_h */

