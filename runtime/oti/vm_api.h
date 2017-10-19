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

#ifndef vm_api_h
#define vm_api_h

/**
* @file vm_api.h
* @brief Public API for the VM module.
*
* This file contains public function prototypes and
* type definitions for the VM module.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "jni.h"
#include "omrthread.h"

#ifdef __cplusplus
extern "C" {
#endif

#define J9_CREATEJAVAVM_VERBOSE_INIT 1
#define J9_CREATEJAVAVM_ARGENCODING_LATIN 2
#define J9_CREATEJAVAVM_ARGENCODING_UTF8 4
#define J9_CREATEJAVAVM_ARGENCODING_PLATFORM 8

typedef struct J9CreateJavaVMParams {
	UDATA j2seVersion;
	char* j2seRootDirectory;
	char* j9libvmDirectory;
	struct J9VMInitArgs* vm_args;
	J9JavaVM **globalJavaVM;
	J9PortLibrary *portLibrary;
	UDATA flags;
} J9CreateJavaVMParams;

/* ---------------- FastJNI.cpp ---------------- */

/**
 * Determine the properties of a JNI native method.
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] jniNativeMethod JNI native method to query
 * @param[out] properties bitfield representing the optimizations to apply to this method
 *
 * @return  The address of the JNI native to encode in the compiled code, or NULL if the method is not a bound JNI native
 */
void*
jniNativeMethodProperties(J9VMThread *currentThread, J9Method *jniNativeMethod, UDATA *properties);

/* ---------------- annsup.c ---------------- */

/**
* @brief
* @param *state
* @param **data
* @return J9UTF8 *
*/
J9UTF8 *
annotationElementIteratorNext(J9AnnotationState *state, void **data);


/**
* @brief
* @param *state
* @param *annotation
* @param **data
* @return J9UTF8 *
*/
J9UTF8 *
annotationElementIteratorStart(J9AnnotationState *state, J9AnnotationInfoEntry *annotation, void **data);


/**
* @brief
* @param *state
* @return void *
*/
void *
elementArrayIteratorNext(J9AnnotationState *state);


/**
* @brief
* @param *state
* @param start
* @param count
* @return void *
*/
void *
elementArrayIteratorStart(J9AnnotationState *state, UDATA start, U_32 count);


/**
* @brief
* @param *annInfo
* @param **annotations
* @return UDATA
*/
UDATA
getAllAnnotationsFromAnnotationInfo(J9AnnotationInfo *annInfo, J9AnnotationInfoEntry **annotations);


/**
* @brief
* @param *currentThread
* @param *containingClass
* @param *annotation
* @param flags
* @return J9AnnotationInfoEntry *
*/
J9AnnotationInfoEntry *
getAnnotationDefaultsForAnnotation(J9VMThread *currentThread, J9Class *containingClass, J9AnnotationInfoEntry *annotation, UDATA flags);


/**
* @brief
* @param *currentThread
* @param *containingClass
* @param *className
* @param classNameLength
* @param flags
* @return J9AnnotationInfoEntry *
*/
J9AnnotationInfoEntry *
getAnnotationDefaultsForNamedAnnotation(J9VMThread *currentThread, J9Class *containingClass, char *className, U_32 classNameLength, UDATA flags);


/**
* @brief
* @param *annInfo
* @param annotationType
* @param *memberName
* @param memberNameLength
* @param *memberSignature
* @param memberSignatureLength
* @param *annotationName
* @param annotationNameLength
* @return J9AnnotationInfoEntry *
*/
J9AnnotationInfoEntry *
getAnnotationFromAnnotationInfo(J9AnnotationInfo *annInfo, UDATA annotationType, char *memberName, U_32 memberNameLength, char *memberSignature, U_32 memberSignatureLength, char *annotationName, U_32 annotationNameLength);


/**
* @brief
* @param *vm
* @param *clazz
* @return J9AnnotationInfo *
*/
J9AnnotationInfo *
getAnnotationInfoFromClass(J9JavaVM *vm, J9Class *clazz);


/**
* @brief
* @param *annInfo
* @param annotationType
* @param *memberName
* @param memberNameLength
* @param *memberSignature
* @param memberSignatureLength
* @param **annotations
* @return UDATA
*/
UDATA
getAnnotationsFromAnnotationInfo(J9AnnotationInfo *annInfo, UDATA annotationType, char *memberName, U_32 memberNameLength, char *memberSignature, U_32 memberSignatureLength, J9AnnotationInfoEntry **annotations);


/**
* @brief
* @param *annotation
* @param *name
* @param nameLength
* @return void *
*/
void *
getNamedElementFromAnnotation(J9AnnotationInfoEntry *annotation, char *name, U_32 nameLength);


/* ---------------- bchelper.c ---------------- */

/**
* @brief
* @param *vmStruct
* @param *classPtr
* @param dimensions
* @param *dimensionArray
* @param allocationType
* @return j9object_t
*/
j9object_t 
helperMultiANewArray(J9VMThread *vmStruct, J9ArrayClass *classPtr, UDATA dimensions, I_32 *dimensionArray, UDATA allocationType);


/* ---------------- bindnatv.c ---------------- */

#if (defined(J9VM_NEEDS_JNI_REDIRECTION))
/**
* @brief
* @param vm
* @param address
* @param classLoader
* @return void *
*/
void *
alignJNIAddress(J9JavaVM * vm, void * address, J9ClassLoader * classLoader);
#endif /* J9VM_NEEDS_JNI_REDIRECTION */


/**
* @brief
* @param javaVM
* @param ramMethod
* @param ramClass
* @param nameOffset
* @return U_8*
*/
U_8*
buildNativeFunctionNames(J9JavaVM * javaVM, J9Method* ramMethod, J9Class* ramClass, UDATA nameOffset);


/**
* @brief
* @param vm
* @return UDATA
*/
UDATA
initializeNativeMethodBindTable(J9JavaVM *vm);


/**
* @brief
* @param vm
* @return void
*/
void
freeNativeMethodBindTable(J9JavaVM *vm);


/**
* @brief
* @param currentThread
* @param nativeMethod
* @param runtimeBind
* @return UDATA
*/
UDATA   
resolveNativeAddress(J9VMThread *currentThread, J9Method *nativeMethod, UDATA runtimeBind);


/* ---------------- classallocation.c ---------------- */

/**
* @brief Get Package ID for the ROM class. NOTE: You must own the class table mutex before calling this function.
* @param *classLoader Classloader for the class
* @param *romClass ROM class with package name
* @param *vmThread J9VMThread instance
* @param entryIndex classpath index
* @param locationType location type of class
* @return UDATA Package ID
*/
UDATA
initializePackageID(J9ClassLoader *classLoader, J9ROMClass *romClass, J9VMThread *vmThread, IDATA entryIndex, I_32 locationType);

/**
* @brief
* @param *javaVM
* @param *classLoaderObject
* @return J9ClassLoader 
*/
J9ClassLoader*
internalAllocateClassLoader(J9JavaVM *javaVM, j9object_t classLoaderObject) ;

/**
* @brief
* @param *javaVM
* @return J9ClassLoader*
*/
J9ClassLoader*
allocateClassLoader(J9JavaVM *javaVM);

/**
* @brief
* @param *classLoader
* @param *javaVM
* @param *vmThread
* @param needsFrameBuild
* @return void
*/
void  
freeClassLoader(J9ClassLoader *classLoader, J9JavaVM *javaVM, J9VMThread *vmThread, UDATA needsFrameBuild);

/* ---------------- classsupport.c ---------------- */



/**
* @brief
* @param vmThread
* @param name
* @param length
* @return UDATA
*/
UDATA
calculateArity(J9VMThread* vmThread, U_8* name, UDATA length);


/**
* @brief
* @param vmThread
* @param romClass
* @param elementClass
* @return J9Class*
*/
J9Class* 
internalCreateArrayClass(J9VMThread* vmThread, J9ROMArrayClass* romClass, J9Class* elementClass);

/**
 * Load the class with the specified name in a given module
 * 
 * @param currentThread Current VM thread
 * @param moduleName j.l.String object representing module name; can be null
 * @param className String object representing name of the class to load
 * @param classLoader J9ClassLoader to use
 * @param options load options such as J9_FINDCLASS_FLAG_EXISTING_ONLY
 * 
 * @return pointer to J9Class if success, NULL if fail
 *
 */
J9Class*  
internalFindClassString(J9VMThread* currentThread, j9object_t moduleName, j9object_t className, J9ClassLoader* classLoader, UDATA options);

/**
 * Load the class with the specified name in the given module.
 * 
 * @param currentThread Current VM thread
 * @param moduleName Pointer to J9Module representing the module containing the class
 * @param className Name of class to load
 * @param classNameLength Length of the class name
 * @param classLoader J9ClassLoader to use
 * @param options load options such as J9_FINDCLASS_FLAG_EXISTING_ONLY
 * 
 * @return pointer to J9Class if success, NULL if fail
 *
 */
J9Class*  
internalFindClassInModule(J9VMThread* vmThread, J9Module* j9module, U_8* className, UDATA classNameLength, J9ClassLoader* classLoader, UDATA options);


/**
* @brief
* @param *romClass
* @return UDATA
*/
UDATA
totalStaticSlotsForClass( J9ROMClass *romClass );


/**
* @brief
* @param *ramClass
* @param *vmThread
* @return void
*/
void
internalRunPreInitInstructions(J9Class * ramClass, J9VMThread * vmThread);


/**
* @brief
* @param vm
* @param index
* @return J9Class*
*/
J9Class*  
resolveKnownClass(J9JavaVM * vm, UDATA index);

/**
* @brief
* @param vm
* @param portLib
* @return J9HashTable*
*/
J9HashTable *
contendedLoadTableNew(J9JavaVM* vm, J9PortLibrary *portLib);

/**
* @brief
* @param vm
*/
void
contendedLoadTableFree(J9JavaVM* vm);

/**
* @brief
* @param clazz
* @param cpIndex
*/
void
fixCPShapeDescription(J9Class * clazz, UDATA cpIndex);

/* ---------------- createramclass.c ---------------- */

/**
 * Use this if you don't have the class path index.
 */
#define J9_CP_INDEX_NONE -1

/**
* @brief
* @param *vmThread
* @param *classLoader
* @param *romClass
* @param options
* @param *elementClass
* @param protectionDomain
* @param **methodRemapArray
* @param entryIndex
* @param locationType
* @return J9Class*
*/
J9Class *   
internalCreateRAMClassFromROMClass(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass,
	UDATA options, J9Class* elementClass, j9object_t protectionDomain, J9ROMMethod ** methodRemapArray,
	IDATA entryIndex, I_32 locationType, J9Class *classBeingRedefined, J9Class *hostClass);
	
/* ---------------- classloadersearch.c ---------------- */

/**
 * @brief	Append a single pathSegment to the bootstrap classloader
 * @param *vm
 * @param *pathSegment				a single path segment specifying the path to be added
 * @param enforceJarRestriction 	when true, fail if the pathSegment is not a jar file
 * @return error code
 */

UDATA
addToBootstrapClassLoaderSearch(J9JavaVM * vm, const char * pathSegment, UDATA classLoaderType, BOOLEAN enforceJarRestriction);

/**
 * @brief Append a single pathSegment to the system classloader
 * @param *vm
 * @param *pathSegment	a single path segment specifying the path to be added
 * @param enforceJarRestriction 	when true, fail if the pathSegment is not a jar file
 *
 * @return error code
 */

UDATA
addToSystemClassLoaderSearch(J9JavaVM * vm, const char* pathSegment, UDATA classLoaderType, BOOLEAN enforceJarRestriction);

/* ---------------- description.c ---------------- */

/**
* @brief
* @param *ramClass
* @param *ramSuperClass
* @param *storage
* @return void
*/
void
calculateInstanceDescription( J9VMThread *vmThread, J9Class *ramClass, J9Class *ramSuperClass, UDATA *storage, J9ROMFieldOffsetWalkState *walkState, J9ROMFieldOffsetWalkResult *walkResult);

#define NO_LOCKWORD_NEEDED (UDATA) -1
#define LOCKWORD_NEEDED		(UDATA) -2

/**
* @brief
* @param vm pointer to J9JavaVM that can be used by the method
* @param *romClass
* @param *ramSuperClass
* @return one of NO_LOCKWORD_NEEDED, LOCKWORD_NEEDED or the offset of the lockword already present in a superclass
*/
UDATA
checkLockwordNeeded( J9JavaVM *vm, J9ROMClass *romClass, J9Class *ramSuperClass, U_32 walkFlags );


/* ---------------- dllsup.c ---------------- */

/**
* @brief
* @param vm
* @param info
* @return UDATA
*/
UDATA loadJ9DLL(J9JavaVM * vm, J9VMDllLoadInfo* info);


/**
* @brief
* @param vm
* @param descriptor
* @param shutdownDueToExit
* @return IDATA
*/
IDATA shutdownDLL(J9JavaVM * vm, UDATA descriptor, UDATA shutdownDueToExit);


/* ---------------- exceptiondescribe.c ---------------- */

/**
* @brief
* @param env
* @return void
*/
void JNICALL   
exceptionDescribe(JNIEnv * env);

void   
internalExceptionDescribe(J9VMThread *vmThread);

/**
* @brief
* @param vmThread
* @param exception
* @param vmThread
* @param userData
* @param method
* @param fileName
* @param lineNumber)
* @param userData
* @param pruneConstructors
* @return UDATA
*/
UDATA
iterateStackTrace(J9VMThread * vmThread, j9object_t* exception,  UDATA  (*callback) (J9VMThread * vmThread, void * userData, J9ROMClass * romClass, J9ROMMethod * romMethod, J9UTF8 * fileName, UDATA lineNumber, J9ClassLoader* classLoader), void * userData, UDATA pruneConstructors);


/* ---------------- exceptionsupport.c ---------------- */



/**
* @brief
* @param *currentThread
* @param *walkState
* @return UDATA
*/
UDATA   
exceptionHandlerSearch(J9VMThread *currentThread, J9StackWalkState *walkState);


/**
* @brief
* @param *currentThread
* @param *thrownExceptionClass
* @param *constantPool
* @param handlerIndex
* @param *walkState
* @return UDATA
*/
UDATA  
isExceptionTypeCaughtByHandler(J9VMThread *currentThread, J9Class *thrownExceptionClass, J9ConstantPool *constantPool, UDATA handlerIndex, J9StackWalkState *walkState);




/**
* @brief
* @param *currentThread
* @param *method
* @return void
*/
void  
setNativeBindOutOfMemoryError(J9VMThread * currentThread, J9Method * method);


/**
* @brief
* @param *currentThread
* @param *method
* @return void
*/
void  
setRecursiveBindError(J9VMThread * currentThread, J9Method * method);


/**
* @brief
* @param *currentThread
* @param *method
* @return void
*/
void  
setNativeNotFoundError(J9VMThread * currentThread, J9Method * method);


/**
* @brief
* @param *currentThread
* @param *initiatingLoader
* @param *existingClass
* @return void
*/
void  
setClassLoadingConstraintError(J9VMThread * currentThread, J9ClassLoader * initiatingLoader, J9Class * existingClass);


/**
* @brief
* @param *currentThread
* @param instanceClass
* @param castClass
* @return void
*/
void  
setClassCastException(J9VMThread *currentThread, J9Class * instanceClass, J9Class * castClass);


/**
* @brief
* @param *currentThread
* @param exceptionNumber
* @param *detailMessage
* @return void
*/
void  
setCurrentException(J9VMThread *currentThread, UDATA exceptionNumber, UDATA *detailMessage);


/**
* @brief
* @param *currentThread
* @param exceptionNumber
* @param *detailMessage
* @param cause
* @return void
*/
void  
setCurrentExceptionWithCause(J9VMThread *currentThread, UDATA exceptionNumber, UDATA *detailMessage, j9object_t cause);

/**
 * @brief Sets the exception using the message and cause
 *
 * @param currentThread current J9VMThread
 * @param exceptionNumber exception to be set
 * @param detailMessage j.l.String object describing the exception
 * @param utfMessage utf8 string describing the exception
 * @param cause cause of exception
 *
 * @return void
 */
void
setCurrentExceptionWithUtfCause(J9VMThread *currentThread, UDATA exceptionNumber, UDATA *detailMessage, const char *utfMessage, j9object_t cause);

/**
* @brief
* @param vmThread
* @param exceptionNumber
* @param moduleName
* @param messageNumber
* @return void
*/
void  
setCurrentExceptionNLS(J9VMThread * vmThread, UDATA exceptionNumber, U_32 moduleName, U_32 messageNumber);

/**
 * @brief Creates exception with nls message; substitutes string values into error message.
 * @param vmThread current VM thread
 * @param nlsModule nls module name
 * @param nlsID nls number
 * @param exceptionIndex index to exception in vm constant pool
 * @param ... arguments to be substituted into error message
 */
void
setCurrentExceptionNLSWithArgs(J9VMThread * vmThread, U_32 nlsModule, U_32 nlsID, UDATA exceptionIndex, ...);

/**
* @brief
* @param currentThread
* @return void
*/
void  
setHeapOutOfMemoryError(J9VMThread * currentThread);


/**
* @brief
* @param currentThread
* @return void
*/
void  
setArrayIndexOutOfBoundsException(J9VMThread * currentThread, IDATA index);


/**
* @brief
* @param vmThread
* @param moduleName
* @param messageNumber
* @return void
*/
void  
setNativeOutOfMemoryError(J9VMThread * vmThread, U_32 moduleName, U_32 messageNumber);


/**
* @brief
* @param vmThread
* @param moduleName
* @param messageNumber
* @return void
*/
void  
setThreadForkOutOfMemoryError(J9VMThread * vmThread, U_32 moduleName, U_32 messageNumber);


/**
* @brief
* @param vmThread - current J9VMThread
* @param method - J9Method representing the conflict method.  (See Jazz 62438)
* @return void
*/
void  
setIncompatibleClassChangeErrorForDefaultConflict(J9VMThread * vmThread, J9Method *method);


/**
* @brief
* @param vmThread - current J9VMThread
* @param method - J9Method* representing the non-public method being invoked.
* @return void
*/
void  
setIllegalAccessErrorNonPublicInvokeInterface(J9VMThread *vmThread, J9Method *method);


/**
 * Error message helper for throwing IllegalAccessError when receiver class is not the same or subtype of current
 * [=interface] class in invokespecial
 *
 * @param vmStruct the current vmThread
 * @param receiverClass represents the receiver class
 * @param currentClass represents the current class
 */
void
setIllegalAccessErrorReceiverNotSameOrSubtypeOfCurrentClass(J9VMThread *vmStruct, J9Class *receiverClass, J9Class *currentClass);

/**
* @brief
* @param currentThread
* @return jint
*/
jint
initializeHeapOOMMessage(J9VMThread *currentThread);

/**
 * Error message helper for throwing IllegalAccessError when illegally accessing final fields.
 *
 * @param currentThread the current vmThread
 * @param isStatic TRUE for static, FALSE for instance
 * @param romClass J9ROMClass of the final field
 * @param field J9ROMFieldShape of the final field
 * @param romMethod J9ROMMethod of the accessing method
 */
void  
setIllegalAccessErrorFinalFieldSet(J9VMThread *currentThread, UDATA isStatic, J9ROMClass *romClass, J9ROMFieldShape *field, J9ROMMethod *romMethod);

/**
* @brief
* @param vmThread
* @param exceptionNumber
* @param detailUTF
* @return void
*/
void  
setCurrentExceptionUTF(J9VMThread * vmThread, UDATA exceptionNumber, const char * detailUTF);


/**
* @brief
* @param currentThread
* @param exception
* @param walkOnly
* @return j9object_t (a java/lang/Throwable)
*/
j9object_t   
walkStackForExceptionThrow(J9VMThread * currentThread, j9object_t exception, UDATA walkOnly);


struct J9Class;
void  
setClassLoadingConstraintSignatureError(J9VMThread *currentThread, J9ClassLoader *loader1, J9Class *class1, J9ClassLoader *loader2, J9Class *class2, J9Class *exceptionClass, U_8 *methodName, UDATA methodNameLength, U_8 *signature, UDATA signatureLength);

void  
setClassLoadingConstraintOverrideError(J9VMThread *currentThread, J9UTF8 *newClassNameUTF, J9ClassLoader *loader1, J9UTF8 *class1NameUTF, J9ClassLoader *loader2, J9UTF8 *class2NameUTF, J9UTF8 *exceptionClassNameUTF, U_8 *methodName, UDATA methodNameLength, U_8 *signature, UDATA signatureLength);

/* ---------------- gphandle.c ---------------- */

struct J9PortLibrary;
/**
* @brief
* @param portLibrary
* @param gpType
* @param gpInfo
* @param userData J9JavaVM pointer.
* @return UDATA
*/
UDATA
structuredSignalHandlerVM(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData);


struct J9PortLibrary;
/**
* @brief
* @param portLibrary
* @param gpType
* @param gpInfo
* @param userData J9VMThread pointer.
* @return UDATA
*/
UDATA
structuredSignalHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData);

/**
* @brief Called  on vmEntry and return from JNI to detect the case where we had percolated a condition and the user native illegally resumed execution
* 	Terminates the VM
* @param vmThread
* @return void
*/
void
javaAndCStacksMustBeInSync(J9VMThread *vmThread, BOOLEAN fromJIT);

/* ---------------- growstack.c ---------------- */

#if (defined(J9VM_INTERP_GROWABLE_STACKS))  /* File Level Build Flags */

/**
* @brief
* @param vmThread
* @param bp
* @return void
*/
void
freeStacks(J9VMThread * vmThread, UDATA * bp);


/**
* @brief
* @param vmThread
* @param newStackSize
* @return UDATA
*/
UDATA  
growJavaStack(J9VMThread * vmThread, UDATA newStackSize);


#endif /* J9VM_INTERP_GROWABLE_STACKS */ /* End File Level Build Flags */


/* ---------------- initsendtarget.c ---------------- */

/**
* @brief
* @param *vmThread
* @param *method
* @param *jxeDescription
* @return void
*/
void
initializeMethodRunAddress(J9VMThread *vmThread, J9Method *method);


/**
* @brief
* @param vm
* @param *method
* @return void
*/
void
initializeMethodRunAddressNoHook(J9JavaVM* vm, J9Method *method);

/**
* @brief
* @param vm
* @return jint
*/
jint
initializeINLInterception(J9JavaVM *vm);

/**
* @brief
* @param vm
* @return void
*/
void
initializeInitialMethods(J9JavaVM *vm);

/* ---------------- jnicsup.c ---------------- */

/**
* @brief
* @param *currentThread
* @param *clazz
* @return void **
*/
void **
ensureJNIIDTable(J9VMThread *currentThread, J9Class* clazz);

/**
* @brief Find or create the corresponding J9JNIFIeldID for the rom field in declaringClass.
*
* This operation may fail if the JNIIDTable for the class cannot be created, there is
* insufficient memory in Classloader's pool of JNIIDs or if the declaringClass has been
* redefined since the J9ROMFieldShape was looked up as this will cause the fieldShape
* not to be present in the declaringClass->romClass
*
* @param *currentThread The current J9VMThread
* @param *declaringClass The J9Class that declares the field
* @param *field The J9ROMFieldShape describing the field.  Must be from the declaringClass->romClass.
* @param offset The offset (object or static) used to read the field
* @param inconsistentData An out parameter that is set to 1 if a redefinition mismatch is detected.  The caller must zero the field before calling.
* @return J9JNIFieldID *  NULL if the operation fails for any reason, including redefinition conflicts or a J9JNIFieldID if it succeeds
*/
J9JNIFieldID *
getJNIFieldID(J9VMThread *currentThread, J9Class* declaringClass, J9ROMFieldShape* field, UDATA offset, UDATA *inconsistentData);

/**
* @brief
* @param *currentThread
* @param *method
* @return J9JNIMethodID *
*/
J9JNIMethodID *
getJNIMethodID(J9VMThread *currentThread, J9Method *method);

/**
* @brief
* @param *currentThread
* @param *methodID
* @param *method
* @return void
*/
void
initializeMethodID(J9VMThread * currentThread, J9JNIMethodID * methodID, J9Method * method);

/**
* @brief
* @param vmStruct
* @param slHandle
* @param fnName
* @param defaultResult
* @return jint
*/
jint
callJNILoad(J9VMThread* vmStruct, UDATA slHandle, char* fnName, jint defaultResult);


/**
* @brief
* @param *env
* @param *msg
* @return void
*/
void JNICALL OMRNORETURN
fatalError(JNIEnv *env, const char *msg);


/**
* @brief
* @param *env
* @param receiver
* @param cls
* @param methodID
* @param args
* @return void
*/
void   
gpCheckCallin(JNIEnv *env, jobject receiver, jclass cls, jmethodID methodID, void* args);


/**
* @brief
* @param env
* @param clazz
* @return void
*/
void JNICALL   
gpCheckInitialize(J9VMThread* env, J9Class* clazz);


/**
* @brief
* @param env
* @param exceptionNumber
* @param detailMessage
* @return void
*/
void JNICALL   
gpCheckSetCurrentException(J9VMThread* env, UDATA exceptionNumber, UDATA* detailMessage);


/**
* @brief
* @param env
* @param exceptionNumber
* @param moduleName
* @param messageNumber
* @return void
*/
void JNICALL   
gpCheckSetCurrentExceptionNLS(J9VMThread* env, UDATA exceptionNumber, U_32 moduleName, U_32 messageNumber);


/**
* @brief
* @param env
* @param moduleName
* @param messageNumber
* @return void
*/
void JNICALL   
gpCheckSetNativeOutOfMemoryError(J9VMThread* env, U_32 moduleName, U_32 messageNumber);


/**
* @brief
* @param env
* @return void
*/
void JNICALL   
gpCheckSetHeapOutOfMemoryError(J9VMThread* env);


/**
* @brief
* @param *vm
* @return void
*/
void
initializeJNITable(J9JavaVM *vm);


/**
* @brief
* @param *env
* @param *object
* @param isWeak
* @return jobject
*/
jobject JNICALL
j9jni_createGlobalRef(JNIEnv *env, j9object_t object, jboolean isWeak);


/**
* @brief
* @param *env
* @param *object
* @return jobject
*/
jobject JNICALL
j9jni_createLocalRef(JNIEnv *env, j9object_t object);


/**
* @brief
* @param *env
* @param globalRef
* @param isWeak
* @return void
*/
void JNICALL
j9jni_deleteGlobalRef(JNIEnv *env, jobject globalRef, jboolean isWeak);


/**
* @brief
* @param *env
* @param localRef
* @return void
*/
void JNICALL
j9jni_deleteLocalRef(JNIEnv *env, jobject localRef);


/**
* @brief
* @param vmThread
* @param type
* @return void
*/
void
jniPopFrame(J9VMThread * vmThread, UDATA type);


/**
* @brief
* @param *vmThread
* @param method
* @param *args
* @return UDATA
*/
UDATA JNICALL  
pushArguments(J9VMThread *vmThread, J9Method* method, void *args);


/* ---------------- jniinv.c ---------------- */

/**
* @brief
* @param vm
* @param p_env
* @param thr_args
* @return jint
*/
jint JNICALL
AttachCurrentThread(JavaVM * vm, void ** p_env, void * thr_args);


/**
* @brief
* @param vm
* @param p_env
* @param thr_args
* @return jint
*/
jint JNICALL
AttachCurrentThreadAsDaemon(JavaVM * vm, void ** p_env, void * thr_args);


/**
* @brief
* @param vm
* @param p_env
* @param threadName
* @return IDATA
*/
IDATA
attachSystemDaemonThread(J9JavaVM * vm, J9VMThread ** p_env, const char * threadName);


/**
* @brief
* @param javaVM
* @return jint
*/
jint JNICALL
DestroyJavaVM(JavaVM * javaVM);


/**
* @brief
* @param javaVM
* @return jint
*/
jint JNICALL
DetachCurrentThread(JavaVM * javaVM);


/**
* @brief
* @param *jvm
* @param **penv
* @param version
* @return jint
*/
jint JNICALL
GetEnv(JavaVM *jvm, void **penv, jint version);


#if (defined(J9VM_OPT_VM_LOCAL_STORAGE))
/**
* @brief
* @param vm
* @return void
*/
void
initializeVMLocalStorage(J9JavaVM * vm);
#endif /* J9VM_OPT_VM_LOCAL_STORAGE */


/**
* @brief
* @param vm
* @param p_env
* @param thr_args
* @param threadType
* @param osThread
* @return IDATA
*/
IDATA
internalAttachCurrentThread(J9JavaVM * vm, J9VMThread ** p_env, J9JavaVMAttachArgs * thr_args, UDATA threadType, void * osThread);


#if (defined(J9VM_OPT_VM_LOCAL_STORAGE))
/**
* @brief
* @param env
* @param pInitCount
* @param ...
* @return UDATA
*/
UDATA JNICALL
J9VMLSAllocKeys(JNIEnv * env, UDATA * pInitCount, ...);
#endif /* J9VM_OPT_VM_LOCAL_STORAGE */


#if (defined(J9VM_OPT_VM_LOCAL_STORAGE))
/**
* @brief
* @param env
* @param pInitCount
* @param ...
* @return void
*/
void JNICALL
J9VMLSFreeKeys(JNIEnv * env, UDATA * pInitCount, ...);
#endif /* J9VM_OPT_VM_LOCAL_STORAGE */


#if (defined(J9VM_OPT_VM_LOCAL_STORAGE))
/**
* @brief
* @param env
* @param key
* @return void *
*/
void * JNICALL
J9VMLSGet(JNIEnv * env, void * key);
#endif /* J9VM_OPT_VM_LOCAL_STORAGE */


#if (defined(J9VM_OPT_VM_LOCAL_STORAGE))
/**
* @brief
* @param env
* @param pKey
* @param value
* @return void *
*/
void * JNICALL
J9VMLSSet(JNIEnv * env, void ** pKey, void * value);
#endif /* J9VM_OPT_VM_LOCAL_STORAGE */


#if (defined(J9VM_OPT_SIDECAR))
/**
* @brief
* @param *vm
* @param *nHeaps
* @param *status
* @param statusSize
* @return jint
*/
jint JNICALL
QueryGCStatus(JavaVM *vm, jint *nHeaps, GCStatus *status, jint statusSize);
#endif /* J9VM_OPT_SIDECAR */


#if (defined(J9VM_OPT_SIDECAR))
/**
* @brief
* @param *vm
* @param nQueries
* @param *queries
* @return jint
*/
jint JNICALL
QueryJavaVM(JavaVM *vm,  jint nQueries, JavaVMQuery *queries);
#endif /* J9VM_OPT_SIDECAR */


#if (defined(J9VM_OPT_SIDECAR))
/**
* @brief
* @param *vm
* @return jint
*/
jint JNICALL
ResetJavaVM(JavaVM *vm);
#endif /* J9VM_OPT_SIDECAR */


/* ---------------- jnimem.c ---------------- */

#if (defined(J9VM_GC_JNI_ARRAY_CACHE))
/**
* @brief
* @param *vmThread
* @return void
*/
void
cleanupVMThreadJniArrayCache(J9VMThread *vmThread);
#endif /* J9VM_GC_JNI_ARRAY_CACHE */


/**
* @brief
* @param vmThread
* @param sizeInBytes
* @return void*
*/
void*
jniArrayAllocateMemoryFromThread(J9VMThread* vmThread, UDATA sizeInBytes);


/**
* @brief
* @param vmThread
* @param location
* @return void
*/
void
jniArrayFreeMemoryFromThread(J9VMThread* vmThread, void* location);

/* ---------------- jvmfree.c ---------------- */

#if (defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING))
/**
* @brief
* @param *vmThread
* @param classLoader
* @return void
*/
void
cleanUpClassLoader(J9VMThread *vmThread, J9ClassLoader* classLoader);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

/**
* @brief
* @param vmThread
* @param decrementZombieCount
* @param sendThreadDestroyEvent
* @return void
*/
void
deallocateVMThread(J9VMThread * vmThread, UDATA decrementZombieCount, UDATA sendThreadDestroyEvent);


/**
* @brief
* @param vmThread
* @param entries
* @param count
* @return void
*/
void
freeClassLoaderEntries(J9VMThread * vmThread, J9ClassPathEntry * entries, UDATA count);

/**
* @brief
* @param vmThread
* @param entries
* @param count
* @return void
*/
void
freeSharedCacheCLEntries(J9VMThread * vmThread, J9ClassLoader * classloader);

/**
 * Free resources held by J9Module and remove it from the pool.
 *
 * @param [in] vmThread pointer to J9VMThread
 * @param [in] j9module module to be freed
 *
 * @return void
 */
void
freeJ9Module(J9JavaVM *javaVM, J9Module *j9module);

/* ---------------- jvminit.c ---------------- */

#if defined(COUNT_BYTECODE_PAIRS)
/**
* @brief
* @param vm
* @return void
*/
void
printBytecodePairs(J9JavaVM *vm);
#endif /* COUNT_BYTECODE_PAIRS */

/**
* @brief
* @param vmThread
* @param rc
* @return void
*/
void OMRNORETURN
exitJavaVM(J9VMThread * vmThread, IDATA rc);

/**
* @brief
* @param j9vm_args
* @param match
* @param optionName
* @param optionValue
* @param doConsumeArgs
* @return IDATA
*/
IDATA
findArgInVMArgs(J9PortLibrary *portLibrary, J9VMInitArgs* j9vm_args, UDATA match, const char* optionName, const char* optionValue, UDATA doConsumeArgs);


/**
* @brief
* @param aPool
* @param dllName
* @return J9VMDllLoadInfo *
*/
J9VMDllLoadInfo *
findDllLoadInfo(J9Pool* aPool, const char* dllName);


/**
* @brief
* @param vm
* @return void
*/
void
freeJavaVM(J9JavaVM * vm);

/**
* @brief
* @param osMainThread
* @param createParams Argument list, port library, etc.
* @return jint
*/
jint
initializeJavaVM(void * osMainThread, J9JavaVM ** vmPtr, J9CreateJavaVMParams *createParams);

/**
* @brief
* @param jniVersion
* @return UDATA
*/
UDATA
jniVersionIsValid(UDATA jniVersion);


/**
* @brief
* @param j9vm_args
* @param element
* @param action
* @param valuesBuffer
* @param bufSize
* @param delim
* @param separator
* @param reserved
* @return IDATA
*/
IDATA
optionValueOperations(J9PortLibrary *portLibrary, J9VMInitArgs* j9vm_args, IDATA element, IDATA action, char** valuesBuffer, UDATA bufSize, char delim, char separator, void* reserved);


/**
* @brief
* @param vm
* @param dllName
* @param argData
* @return IDATA
*/
IDATA
postInitLoadJ9DLL(J9JavaVM* vm, const char* dllName, void* argData);


#if (defined(J9VM_OPT_SIDECAR))
/**
* @brief
* @param vm
* @param sov_option
* @param j9_option
* @param mapFlags
* @return IDATA
*/
IDATA
registerCmdLineMapping(J9JavaVM* vm, char* sov_option, char* j9_option, UDATA mapFlags);
#endif /* J9VM_OPT_SIDECAR */


/**
* @brief
* @param vm
* @return void
*/
void
runExitStages(J9JavaVM* vm, J9VMThread * vmThread);


/**
* @brief
* @param vm
* @param loadInfo
* @param options
* @return UDATA
*/
UDATA
runJVMOnLoad(J9JavaVM* vm, J9VMDllLoadInfo* loadInfo, char* options);


/**
* @brief
* @param vm
* @param stage
* @param reserved
* @return IDATA
*/
IDATA
threadInitStages(J9JavaVM* vm, IDATA stage, void* reserved);


/**
* @brief
* @param *vm
* @param stage
* @param reserved
* @return IDATA
*/
IDATA
VMInitStages(J9JavaVM *vm, IDATA stage, void* reserved);

/**
 * Initializes class path entries. Path of each class path entry is set using the 'classPath' provided as paramenter.
 * Each path in 'classPath' is seaprated by 'classPathSeparator'.
 * If 'initClassPathEntry' is TRUE, then each class path entry in initialized by calling 'initializeClassPathEntry()' function.
 *
 * @param[in] vm pointer to the J9JavaVM
 * @param[in] classPath set of paths separated by 'classPathSeparator'
 * @param[in] classPathSeparator platform specific class path separator
 * @param[in] cpFlags clas path entry flags
 * @param[in] initClassPathEntry if TRUE initialize each class path entry
 * @param[out] classPathEntries returns the class path entries allocated and initialized by the function
 *
 * @return number of class path entries initialized
 */
UDATA
initializeClassPath(J9JavaVM *vm, char *classPath, U_8 classPathSeparator, U_16 cpFlags, BOOLEAN initClassPathEntry, J9ClassPathEntry **classPathEntries);

/*
 * Initialize the given class path entry. This involves determining what kind
 * of entry it is, and preparing to do class loads (without actually doing any).
 *
 * @param[in] javaVM pointer to the J9JavaVM
 * @param[in] cpEntry pointer to J9ClassPathEntry to be initialized
 *
 * @return IDATA type of the entry which is one of the following
 * 		CPE_TYPE_DIRECTORY if its a a directory
 * 		CPE_TYPE_JAR if its a ZIP file, extraInfo contains the J9ZipFile
 * 		CPE_TYPE_JIMAGE if its a jimage file
 * 		CPE_TYPE_USUSABLE if its a bad entry, don't try to use it anymore
 */
IDATA
initializeClassPathEntry (J9JavaVM * javaVM, J9ClassPathEntry *cpEntry);

/**
 * Searches system properties for jdk.launcher.patch.<n> property that specifies
 * the patch path for the given moduleName.
 * It should only be called for modules loaded by bootloader.
 * moduleName should never be NULL.
 *
 * @param[in] javaVM		pointer to the J9JavaVM
 * @param[in] j9module		pointer to J9Module for which patch path is to be set
 * @param[in] moduleName	name of the module
 * @return TRUE on success, FALSE if an internal error occurs 
 */
BOOLEAN
setBootLoaderModulePatchPaths(J9JavaVM * javaVM, J9Module * j9module, const char * moduleName);


/* ---------------- romutil.c ---------------- */

/**
* @brief
* @param *romClass
* @param *vmThread
* @return J9ROMClass*
*/
J9ROMClass*
checkRomClassForError( J9ROMClass *romClass, J9VMThread *vmThread );

struct J9ClassLoader;
/**
* @brief
* @param *vmStruct
* @param *clsName
* @param clsNameLength
* @param *romClassBytes
* @param romClassLength
* @return J9ROMClass *
*/
J9ROMClass *
romClassLoadFromCookie (J9VMThread *vmStruct, U_8 *clsName, UDATA clsNameLength, U_8 *romClassBytes, UDATA romClassLength);


/**
* @brief
* @param *romClass
* @param *vmThread
* @return void
*/
void
setExceptionForErroredRomClass( J9ROMClass *romClass, J9VMThread *vmThread );


/* ---------------- KeyHashTable.c ---------------- */

/**
* Searches classLoader for any loaded classes in a specific package
*
* @param classLoader loader to be searched
* @param pkgName package name
* @param pkgNameLength package name length
* @return TRUE if class has been loaded in package, FALSE otherwise
*/
BOOLEAN
isAnyClassLoadedFromPackage(J9ClassLoader* classLoader, U_8 *pkgName, UDATA pkgNameLength);

/**
* @brief
* @param *classLoader
* @return void
*/
void
hashClassTableFree(J9ClassLoader* classLoader);


/**
* @brief
* @param *classLoader
* @param *className
* @param classNameLength
* @return J9Class *
*/
J9Class *
hashClassTableAt(J9ClassLoader *classLoader, U_8 *className, UDATA classNameLength);


/**
* @brief
* @param *classLoader
* @param stringObject
* @return J9Class *
*/
J9Class *
hashClassTableAtString(J9ClassLoader *classLoader, j9object_t stringObject);


/**
* @brief
* @param *classLoader
* @param *className
* @param classNameLength
* @param *value
* @return UDATA
*/
UDATA
hashClassTableAtPut(J9VMThread *vmThread, J9ClassLoader *classLoader, U_8 *className, UDATA classNameLength, J9Class *value);


/**
* @brief
* @param *table
* @param *className
* @param classNameLength
* @return UDATA
*/
UDATA
hashClassTableDelete(J9ClassLoader *classLoader, U_8 *className, UDATA classNameLength);

/**
* @brief
* @param *javaVM
* @param initialSize
* @return J9HashTable *
*/
J9HashTable *
hashClassTableNew(J9JavaVM *javaVM, U_32 initialSize);


/**
* @brief
* @param vmThread
* @param *classLoader
* @param *originalClass
* @param *replacementClass
* @return void
*/
void
hashClassTableReplace(J9VMThread* vmThread, J9ClassLoader *classLoader, J9Class *originalClass, J9Class *replacementClass);

/**
* @brief Iterate over the classes defined by or cached in the specified class loader
* @param *classLoader
* @param *walkState
* @return J9Class*
*/
J9Class*
hashClassTableStartDo(J9ClassLoader *classLoader, J9HashTableState* walkState);

/**
* @brief Iterate over the classes defined by or cached in the specified class loader
* @param *walkState
* @return J9Class*
*/
J9Class*
hashClassTableNextDo(J9HashTableState* walkState);

/**
 * @brief Create the hash table to map J9Class to J9ClassLocation
 * @param initialSize initial size
 * @return Pointer to new hash table
 */
J9HashTable *
hashClassLocationTableNew(J9JavaVM *javaVM, U_32 initialSize);

/**
 * @brief Locates and returns a structure containing load locatioin for the given class 
 * Caller must acquire classLoaderModuleAndLocationMutex before making the call
 *
 * @param currentThread current thread pointer
 * @param clazz J9Class for which load location is to be searched
 *
 * @return pointer to J9ClassLocatioin for the given class, or NULL if not found
 */
J9ClassLocation *
findClassLocationForClass(J9VMThread *currentThread, J9Class *clazz);

/* ---------------- ModularityHashTables.c ---------------- */

/**
 * @brief Create the module definition hash table
 * @param initialSize initial size
 * @return Pointer to new hash table
 */
J9HashTable *
hashModuleTableNew(J9JavaVM *javaVM, U_32 initialSize);

/**
 * @brief Create the package definition hash table
 * @param initialSize initial size
 * @return Pointer to new hash table
 */
J9HashTable *
hashPackageTableNew(J9JavaVM *javaVM, U_32 initialSize);

/**
 * @brief Create the hash table to map J9Module to J9ModuleExtraInfo
 * @param initialSize initial size
 * @return Pointer to new hash table
 */
J9HashTable *
hashModuleExtraInfoTableNew(J9JavaVM *javaVM, U_32 initialSize);

/**
 * @brief Returns pointer to J9Module that is associated with the given package name
 * @param currentThread current thread pointer
 * @param classLoader the search will be limited to this class loader
 * @param packageName pointer to J9UTF8 containing the package name
 * @return Returns pointer to J9Module that is associated with the given package name or NULL if not found.
 */
J9Module *
findModuleForPackageUTF8(J9VMThread *currentThread, J9ClassLoader *classLoader, J9UTF8 *packageName);

/**
 * @brief Returns pointer to J9Module that is associated with the given package name
 * @param currentThread current thread pointer
 * @param classLoader the search will be limited to this class loader
 * @param packageName the package name
 * @param packageNameLen length of the package name
 * @return Returns pointer to J9Module that is associated with the given package name or NULL if not found.
 */
J9Module *
findModuleForPackage(J9VMThread *currentThread, J9ClassLoader *classLoader, U_8 *packageName, U_32 packageNameLen);

/**
 * @brief Locates and returns a structure containing extra information like jrt URL, patch path entries associated with the given module
 *
 * @param currentThread current thread pointer
 * @param classLoader the search will be limited to this class loader's table
 * @param j9module module for which J9ModuleExtraInfo is required
 *
 * @return pointer to J9ModuleExtraInfo for the given module, or NULL if not found
 */
J9ModuleExtraInfo *
findModuleInfoForModule(J9VMThread *currentThread, J9ClassLoader *classLoader, J9Module *j9module);

/* ---------------- lookupmethod.c ---------------- */

/**
 * @brief Search method in target class
 *
 * @param clazz The class or interface to start the search
 * @param name The name of the method
 * @param nameLength The length of the method name
 * @param sig The signature of the method
 * @param sigLength The length of the method signature
 *
 * @returns the method (if found) or NULL otherwise
 */
J9Method* 
searchClassForMethod(J9Class *clazz, U_8 *name, UDATA nameLength, U_8 *sig, UDATA sigLength) ;

/**
 * Redirects to javaLookupMethodImpl with foundDefaultConflicts = NULL.
 * 
 * @brief
 * @param currentThread
 * @param method
 * @return J9Method*
 */
UDATA 
javaLookupMethod (J9VMThread *vmContext, J9Class *clazz, J9ROMNameAndSignature *selector, J9Class *senderClass, UDATA options);

/**
 * Temporary impl of javaLookupMethod for adding BOOLEAN foundDefConflicts. Will be removed as part of work item 67075.
 */
UDATA
javaLookupMethodImpl (J9VMThread *vmContext, J9Class *clazz, J9ROMNameAndSignature *selector, J9Class *senderClass, UDATA options, BOOLEAN *foundDefaultConflicts);

/* ---------------- lookuphelper.c ---------------- */

/**
* @brief
* @param currentThread
* @param method
* @return J9Method*
*/
J9Method*
getForwardedMethod(J9VMThread* currentThread, J9Method* method);


/**
* @brief
* @param vm
* @return UDATA
*/
UDATA
mustReportEnterStepOrBreakpoint(J9JavaVM * vm);


/* ---------------- monhelpers.c ---------------- */

/**
* @brief
* @param vmStruct
* @param object
* @return IDATA
*/
IDATA
objectMonitorEnter(J9VMThread* vmStruct, j9object_t object);


/**
* @brief
* @param vmStruct
* @param object
* @return IDATA
*/
IDATA
objectMonitorExit(J9VMThread* vmStruct, j9object_t object);


/**
* @brief
* @param vmStruct
* @param object
* @param lock
* @return omrthread_monitor_t
*/
J9ObjectMonitor *
objectMonitorInflate(J9VMThread* vmStruct, j9object_t object, UDATA lock);

/**
* @brief
* @param *vm
* @param vmThread
* @param monitor
* @return IDATA
*/
IDATA
objectMonitorDestroy(J9JavaVM *vm, J9VMThread *vmThread, omrthread_monitor_t monitor);

/**
* @brief
* @param vm
* @param vmThread
*/
void
objectMonitorDestroyComplete(J9JavaVM *vm, J9VMThread *vmThread);

#if defined(J9VM_THR_LOCK_RESERVATION)
/**
 * Cancel the lock reservation for the object stored in the current thread's
 * blockingEnterObject field.
 *
 * The current thread must have VM access.
 * The current thread must not own the inflated (bimodal) monitor for the object.
 *
 * This function may release and reacquire VM access.
 *
 * On return the object's lock word is unreserved but not assigned to the current thread. The thread
 * must still acquire the lock using the normal process. It is also possible that the lock has been reserved
 * again by the time the caller sees it.
 */
void
cancelLockReservation(J9VMThread* vmStruct);

#endif /* J9VM_THR_LOCK_RESERVATION */

/* ---------------- montable.c ---------------- */

/**
* @brief
* @param vm
* @return void
*/
void
destroyMonitorTable(J9JavaVM* vm);


/**
* @brief
* @param vm
* @return UDATA
*/
UDATA
initializeMonitorTable(J9JavaVM* vm);


/**
* @brief
* @param vmStruct
* @param object
* @return omrthread_monitor_t
*/
J9ObjectMonitor *
monitorTableAt(J9VMThread* vmStruct, j9object_t object);


#ifdef J9VM_THR_LOCK_NURSERY
/**
* @brief used to cache an J9ObjectMonitor in the vmthread structure so that
*        we don't have to go to the monitor table to get it
* @param vm  J9JavaVM that can be used by the method
* @param vmStruct the vmThread for which the object monitor should be cached
* @param objectMonitor the J9ObjectMonitor to be cached on the thread
* @return void
*/
void
cacheObjectMonitorForLookup(J9JavaVM* vm, J9VMThread* vmStruct, J9ObjectMonitor* objectMonitor);

#endif

/* ---------------- PackageIDHashTable.c ---------------- */

/**
* @brief Find the package ID for the given name and length
* @param *vmThread J9VMThread instance
* @param *classLoader Classloader for the class
* @param *romClass ROM class with the package name
* @param entryIndex classpath index
* @param locationType location type of class
* @return UDATA Package ID
*/
UDATA
hashPkgTableIDFor(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass* romClass, IDATA entryIndex, I_32 locationType);

/**
* @brief Look up the package information given a ROM class.
* @note the ROM class may be a dummy with only  the package name.
* @param *classLoader Classloader for the class
* @param *romClass ROM class with the package name
* @return J9PackageIDTableEntry* ROM class representing a package
*/
J9PackageIDTableEntry *
hashPkgTableAt(J9ClassLoader* classLoader, J9ROMClass* romClass);

/**
* @brief Iterate over the package IDs used by the specified class loader
* @param *classLoader
* @param *walkState
* @return J9PackageIDTableEntry*
*/
J9PackageIDTableEntry*
hashPkgTableStartDo(J9ClassLoader *classLoader, J9HashTableState* walkState);

/**
* @brief Iterate over the package IDs used by the specified class loader
* @param *walkState
* @return J9PackageIDTableEntry*
*/
J9PackageIDTableEntry*
hashPkgTableNextDo(J9HashTableState* walkState);

/* ---------------- profilingbc.c ---------------- */

#if (defined(J9VM_INTERP_PROFILING_BYTECODES))
/**
* @brief
* @param vmThread
* @return void
*/
void
flushBytecodeProfilingData(J9VMThread* vmThread);
#endif /* J9VM_INTERP_PROFILING_BYTECODES */


#if (defined(J9VM_INTERP_PROFILING_BYTECODES))
/**
* @brief
* @param vm
* @return void
*/
void
profilingBytecodeBufferFullHookRegistered(J9JavaVM* vm);

#endif /* J9VM_INTERP_PROFILING_BYTECODES */


/* ---------------- rasdump.c ---------------- */

#if (defined(J9VM_RAS_DUMP_AGENTS))
/**
* @brief
* @param *vm
* @return IDATA
*/
IDATA
configureRasDump(J9JavaVM *vm);
#endif /* J9VM_RAS_DUMP_AGENTS */


struct J9JavaVM;
struct J9VMThread;
/**
* @brief
* @param *vm
* @param *currentThread
* @return IDATA
*/
IDATA
gpThreadDump(struct J9JavaVM *vm, struct J9VMThread *currentThread);


/* ---------------- rastrace.c ---------------- */

/**
* @brief
* @param *vm
* @param *j9vm_args
* @return IDATA
*/
IDATA
configureRasTrace(J9JavaVM *vm, J9VMInitArgs *j9vm_args);

/* ---------------- resolvefield.c ---------------- */

/**
* @brief
* @param vm
* @return UDATA
*/
UDATA
initializeHiddenInstanceFieldsList(J9JavaVM *vm);

/**
* @brief
* @param vm
* @return void
*/
void
freeHiddenInstanceFieldsList(J9JavaVM *vm);

/**
 * @brief Add an extra hidden instance field to the specified class when it is loaded.
 * @param vm[in] pointer to the J9JavaVM
 * @param className[in] the fully qualified name of the class to be modified
 * @param fieldName[in] the name of the hidden field to be added (used for information purposes only - no checks for duplicate names will be made)
 * @param fieldSignature[in] the Java signature for the field (e.g. "J" (long), "I" (int), "Ljava/lang/String;", etc)
 * @param offsetReturn[out] the address to store the offset of the field once the class is loaded; may be NULL if not needed
 * @return 0 on success, non-zero if the field could not be recorded or some other error occurred
 */
UDATA
addHiddenInstanceField(J9JavaVM *vm, const char *className, const char *fieldName, const char *fieldSignature, UDATA *offsetReturn);

/**
* @brief
* @param *vmStruct
* @param *clazz
* @param *fieldName
* @param fieldNameLength
* @param *signature
* @param signatureLength
* @param **definingClass
* @param *instanceField
* @param options
* @return IDATA
*/
IDATA
instanceFieldOffset(J9VMThread *vmStruct, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, J9Class **definingClass, UDATA *instanceField, UDATA options);


/**
* @brief
* @param *vmStruct
* @param *clazz
* @param *fieldName
* @param fieldNameLength
* @param *signature
* @param signatureLength
* @param **definingClass
* @param *instanceField
* @param options
* @param *sourceClass
* @return IDATA
*/
IDATA
instanceFieldOffsetWithSourceClass(J9VMThread *vmStruct, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, J9Class **definingClass, UDATA *instanceField, UDATA options, J9Class *sourceClass);

/**
* @brief Iterate over fields of the specified class in JVMTI order.
* @param vm[in]			pointer to the J9JavaVM
* @param romClass[in]	the ROM class whose fields will be iterated
* @param superClazz[in] the RAM super class of the class whose fields will be iterated
* @param state[in/out]  the walk state that can subsequently be passed to fieldOffsetsNextDo()
* @param flags[in]		J9VM_FIELD_OFFSET_WALK_* flags
* @return J9ROMFieldOffsetWalkResult *
*/
J9ROMFieldOffsetWalkResult *
fieldOffsetsStartDo(J9JavaVM *vm, J9ROMClass *romClass, J9Class *superClazz, J9ROMFieldOffsetWalkState *state, U_32 flags);

/**
* @brief Iterate over fields of the specified class in JVMTI order.
* @param state[in/out]  the walk state that was initialised via fieldOffsetsStartDo()
* @return J9ROMFieldOffsetWalkResult *
*/
J9ROMFieldOffsetWalkResult *
fieldOffsetsNextDo(J9ROMFieldOffsetWalkState *state);

/**
* @brief Fully traverse the fields of the specified class and its superclasses.
* @param vm[in]			pointer to the J9JavaVM
* @param clazz[in]		the RAM class whose fields will be iterated
* @param state[in/out]  the walk state that can subsequently be passed to fullTraversalFieldOffsetsNextDo()
* @param flags[in]		J9VM_FIELD_OFFSET_WALK_* flags
* @return J9ROMFieldShape *
*/
J9ROMFieldShape *
fullTraversalFieldOffsetsStartDo(J9JavaVM *vm, J9Class *clazz, J9ROMFullTraversalFieldOffsetWalkState *state, U_32 flags);

/**
* @brief Fully traverse the fields of the specified class and its superclasses.
* @param state[in/out]  the walk state that was initialised via fullTraversalFieldOffsetsStartDo()
* @return J9ROMFieldShape *
*/
J9ROMFieldShape *
fullTraversalFieldOffsetsNextDo(J9ROMFullTraversalFieldOffsetWalkState *state);

/**
* @brief
* @param *vmStruct
* @param *clazz
* @param *fieldName
* @param fieldNameLength
* @param *signature
* @param signatureLength
* @param **definingClass
* @param *staticField
* @param options
* @param *sourceClass
* @return void *
*/
void *  
staticFieldAddress(J9VMThread *vmStruct, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, J9Class **definingClass, UDATA *staticField, UDATA options, J9Class *sourceClass);

/**
* @brief
* @param *vm: Reference to the VM, used to locate the table.
* @param *ramClass: key for the table
* @returns none
* @exceptions none
*/
void
fieldIndexTableRemove(J9JavaVM* vm, J9Class *ramClass);


/* ---------------- resolvesupport.c ---------------- */
/*
 */
/**
 * Perform a package access check from the ProtectionDomain to the targetClass
 * No check is required if no SecurityManager is in place.  If a check is required and the
 * current thread is not able to run java code, the check will be considered to have failed.
 * If the check is aborted due to pop frame, the check will be considered to have failed.
 *
 * @param *currentThread the current J9VMThread
 * @param *targetClass the J9Class which might be in a restricted package
 * @param protectionDomain the ProtectionDomain of the class attempting to access targetClass
 * @param canRunJavaCode non-zero if the current thread is in a state that java code can be executed
 * @return UDATA non-zero if the access is legal
 */
UDATA   
packageAccessIsLegal(J9VMThread *currentThread, J9Class *targetClass, j9object_t protectionDomain, UDATA canRunJavaCode);

/**
 * @brief
 * @param *vmStruct
 * @param *constantPool
 * @param cpIndex
 * @return j9object_t
 */
j9object_t   
resolveStringRef (J9VMThread *vmStruct, J9ConstantPool *constantPool, UDATA cpIndex, UDATA resolveFlags);


/**
 * @brief
 * @param *vmStruct
 * @param *ramCP
 * @param cpIndex
 * @param resolveFlags
 * @return J9Class *
 */
J9Class *   
resolveClassRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags);


/**
 * @brief
 * @param *vmStruct
 * @param *ramCP
 * @param fieldRefCpIndex
 * @return J9Class *
 */
J9Class *   
findFieldSignatureClass(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA fieldRefCpIndex);


/**
 * @brief
 * @param *vmStruct
 * @param *ramCP
 * @param cpIndex
 * @param resolveFlags
 * @param *ramCPEntry
 * @return J9Method *
 */
J9Method *   
resolveStaticMethodRefInto(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9RAMStaticMethodRef *ramCPEntry);


/**
 * @brief
 * @param *vmStruct
 * @param *ramCP
 * @param cpIndex
 * @param resolveFlags
 * @return J9Method *
 */
J9Method *   
resolveStaticMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags);


#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
/**
 * @brief
 * @param *vmStruct
 * @param *ramCP
 * @param splitTableIndex
 * @param resolveFlags
 * @return J9Method *
 */
J9Method *   
resolveStaticSplitMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA splitTableIndex, UDATA resolveFlags);
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */

/**
 * @brief
 * @param *vmStruct
 * @param *method
 * @param *ramCP
 * @param cpIndex
 * @param resolveFlags
 * @param **resolvedField
 * @param *ramCPEntry
 * @return void *
 */
void *    
resolveStaticFieldRefInto(J9VMThread *vmStruct, J9Method *method, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9ROMFieldShape **resolvedField, J9RAMStaticFieldRef *ramCPEntry);


/**
 * @brief
 * @param *vmStruct
 * @param *method
 * @param *ramCP
 * @param cpIndex
 * @param resolveFlags
 * @param **resolvedField
 * @return void *
 */
void *   
resolveStaticFieldRef(J9VMThread *vmStruct, J9Method *method, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9ROMFieldShape **resolvedField);


/**
 * @brief
 * @param *vmStruct
 * @param *method
 * @param *ramCP
 * @param cpIndex
 * @param resolveFlags
 * @param **resolvedField
 * @param *ramCPEntry
 * @return IDATA
 */
IDATA   
resolveInstanceFieldRefInto(J9VMThread *vmStruct, J9Method *method, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9ROMFieldShape **resolvedField, J9RAMFieldRef *ramCPEntry);


/**
 * @brief
 * @param *vmStruct
 * @param *method
 * @param *ramCP
 * @param cpIndex
 * @param resolveFlags
 * @param **resolvedField
 * @return IDATA
 */
IDATA   
resolveInstanceFieldRef(J9VMThread *vmStruct, J9Method *method, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9ROMFieldShape **resolvedField);


/**
 * @brief
 * @param *vmStruct
 * @param *ramCP
 * @param cpIndex
 * @param resolveFlags
 * @param *ramCPEntry
 * @return UDATA
 */
J9Method *   
resolveInterfaceMethodRefInto(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9RAMInterfaceMethodRef *ramCPEntry);


/**
 * @brief
 * @param *vmStruct
 * @param *ramCP
 * @param cpIndex
 * @param resolveFlags
 * @return UDATA
 */
J9Method *   
resolveInterfaceMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags);


/**
 * @brief
 * @param *vmStruct
 * @param *ramCP
 * @param cpIndex
 * @param resolveFlags
 * @param *ramCPEntry
 * @return J9Method *
 */
J9Method *   
resolveSpecialMethodRefInto(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9RAMSpecialMethodRef *ramCPEntry);


/**
 * @brief
 * @param *vmStruct
 * @param *ramCP
 * @param cpIndex
 * @param resolveFlags
 * @return J9Method *
 */
J9Method *   
resolveSpecialMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags);

#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
/**
 * @brief
 * @param *vmStruct
 * @param *ramCP
 * @param splitTableIndex
 * @param resolveFlags
 * @return J9Method *
 */
J9Method *   
resolveSpecialSplitMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA splitTableIndex, UDATA resolveFlags);
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */

/**
 * @brief
 * @param *vmStruct
 * @param *ramCP
 * @param cpIndex
 * @param resolveFlags
 * @param *ramCPEntry
 * @return UDATA
 */
UDATA   
resolveVirtualMethodRefInto(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9Method **resolvedMethod, J9RAMVirtualMethodRef *ramCPEntry);


/**
 * @brief
 * @param *vmStruct
 * @param *ramCP
 * @param cpIndex
 * @param resolveFlags
 * @return UDATA
 */
UDATA   
resolveVirtualMethodRef(J9VMThread *vmStruct, J9ConstantPool *ramCP, UDATA cpIndex, UDATA resolveFlags, J9Method **resolvedMethod);


/* ---------------- segment.c ---------------- */

/**
* @brief
* @param state
* @return void
*/
void
allClassesEndDo(J9ClassWalkState* state);


/**
* @brief
* @param state
* @return J9Class*
*/
J9Class*
allClassesNextDo(J9ClassWalkState* state);


/**
* @brief
* @param state
* @param vm
* @param classLoader
* @return J9Class*
*/
J9Class*
allClassesStartDo(J9ClassWalkState* state, J9JavaVM* vm, J9ClassLoader* classLoader);

/**
* @brief
* @param state
* @return void
*/
void
allLiveClassesEndDo(J9ClassWalkState* state);


/**
* @brief
* @param state
* @return J9Class*
*/
J9Class*
allLiveClassesNextDo(J9ClassWalkState* state);


/**
* @brief
* @param state
* @param vm
* @param classLoader
* @return J9Class*
*/
J9Class*
allLiveClassesStartDo(J9ClassWalkState* state, J9JavaVM* vm, J9ClassLoader* classLoader);

/**
* @brief
* @param state
* @return void
*/
void
allClassLoadersEndDo(J9ClassLoaderWalkState* state);


/**
* @brief
* @param state
* @return J9ClassLoader*
*/
J9ClassLoader*
allClassLoadersNextDo(J9ClassLoaderWalkState* state);


/**
* @brief
* @param state
* @param vm
* @param flags
* @return J9ClassLoader*
*/
J9ClassLoader*
allClassLoadersStartDo(J9ClassLoaderWalkState* state, J9JavaVM* vm, UDATA flags);

/* J9ClassLoaderWalkState flags */
#define J9CLASSLOADERWALK_INCLUDE_DEAD 1
#define J9CLASSLOADERWALK_EXCLUDE_LIVE 2

/**
* @brief
* @param *javaVM
* @param size
* @param type
* @param memoryCategory
* @return J9MemorySegment *
*/
J9MemorySegment *
allocateMemorySegment(J9JavaVM *javaVM, UDATA size, UDATA type, U_32 memoryCategory);


/**
* @brief
* @param *javaVM
* @param *segmentList
* @param size
* @param type
* @param memoryCategory
* @return J9MemorySegment *
*/
J9MemorySegment *
allocateMemorySegmentInList(J9JavaVM *javaVM, J9MemorySegmentList *segmentList, UDATA size, UDATA type, U_32 memoryCategory);


/**
* @brief
* @param *javaVM
* @param *segmentList
* @param size
* @param type
* @param *vmemParams
* @return J9MemorySegment *
*/
J9MemorySegment *
allocateVirtualMemorySegmentInList(J9JavaVM *javaVM, J9MemorySegmentList *segmentList, UDATA size, UDATA type, J9PortVmemParams *vmemParams);


/**
* @brief
* @param *javaVM
* @param requiredSize
* @param segmentType
* @param *classLoader
* @return allocationIncrement
*/
J9MemorySegment *
allocateClassMemorySegment(J9JavaVM *javaVM, UDATA requiredSize, UDATA segmentType, J9ClassLoader *classLoader, UDATA allocationIncrement);


/**
* @brief
* @param *javaVM
* @param *segmentList
* @param size
* @param type
* @param desiredAddress
* @param memoryCategory
* @return J9MemorySegment *
*/
J9MemorySegment *
allocateFixedMemorySegmentInList(J9JavaVM *javaVM, J9MemorySegmentList *segmentList, UDATA size, UDATA type, U_8* desiredAddress, U_32 memoryCategory);


/**
* @brief
* @param javaVM
* @param numberOfMemorySegments
* @param memoryCategory
* @return J9MemorySegmentList *
*/
J9MemorySegmentList *
allocateMemorySegmentList(J9JavaVM * javaVM, U_32 numberOfMemorySegments, U_32 memoryCategory);


/**
* @brief
* @param *segmentList
* @return J9MemorySegment *
*/
J9MemorySegment *
allocateMemorySegmentListEntry(J9MemorySegmentList *segmentList);


/**
* @brief
* @param javaVM
* @param numberOfMemorySegments
* @param sizeOfElements
* @return J9MemorySegmentList *
*/
J9MemorySegmentList *
allocateMemorySegmentListWithSize(J9JavaVM * javaVM, U_32 numberOfMemorySegments, UDATA sizeOfElements, U_32 memoryCategory);


/**
* @brief
* @param *javaVM
* @param *segment
* @param freeDescriptor
* @return void
*/
void
freeMemorySegment(J9JavaVM *javaVM, J9MemorySegment *segment, BOOLEAN freeDescriptor);


/**
* @brief
* @param *javaVM
* @param *segmentList
* @return void
*/
void
freeMemorySegmentList(J9JavaVM *javaVM,J9MemorySegmentList *segmentList);


/**
* @brief
* @param *segmentList
* @param *segment
* @return void
*/
void
freeMemorySegmentListEntry(J9MemorySegmentList *segmentList, J9MemorySegment *segment);


/**
* @brief
* @param *segmentList
* @return U_32
*/
U_32
memorySegmentListSize (J9MemorySegmentList *segmentList);


/**
* @brief
* @param *vm
* @param *segmentList
* @param valueToFind
* @return J9MemorySegment *
*/
J9MemorySegment *
findMemorySegment(J9JavaVM *javaVM, J9MemorySegmentList *segmentList, UDATA valueToFind);

/**
* @brief
* @param javaVM
* @param numberOfMemorySegments
* @param flags
* @param memoryCategory
* @return J9MemorySegmentList *
*/
J9MemorySegmentList *
allocateMemorySegmentListWithFlags(J9JavaVM * javaVM, U_32 numberOfMemorySegments, UDATA flags, U_32 memoryCategory);


/* ---------------- statistics.c ---------------- */
/**
* @brief
* @param *javaVM
* @param *name
* @param dataType
* @return void *
*/
void *
addStatistic (J9JavaVM* javaVM, U_8 * name, U_8 dataType);


/**
* @brief
* @param *javaVM
* @param *name
* @return void *
*/
void *
getStatistic (J9JavaVM* javaVM, U_8 * name);

/* ---------------- stringhelpers.c ---------------- */

/**
 * @brief
 * @param *vmThread
 * @param *data
 * @param length
 * @param stringFlags
 * @param byteArray
 * @param offset
 */
void
copyUTF8ToCompressedUnicode(J9VMThread *vmThread, U_8 *data, UDATA length, UDATA stringFlags, j9object_t byteArray, UDATA offset);

/**
 * @brief Compare a java string to another java string for character equality.
 * @param *vmThread
 * @param *string1
 * @param *string2
 * @returns 1 if the strings are equal, 0 otherwise
 */

UDATA
compareStrings(J9VMThread *vmThread, j9object_t string1, j9object_t string2);

/**
 * @brief 
 * @param *javaVM
 * @param *stringObject
 * @param stringFlags
 * @param *utfData
 * @param utfLength
 * @returns UDATA
 */
UDATA
compareStringToUTF8(J9VMThread * vmThread, j9object_t stringObject, UDATA stringFlags, const U_8 * utfData, UDATA utfLength);


/**
* @brief
* @param *vm
* @param *header
* @param isBaseType
* @param *classLoader
* @return J9MemorySegment *
*/
J9MemorySegment *
romImageNewSegment(J9JavaVM *vm, J9ROMImageHeader *header, UDATA isBaseType, J9ClassLoader *classLoader );


/**
 * Copy a string object to a UTF8 data buffer, optionally to prepend a string before it
 *
 * ***The caller must free the memory from this pointer if the return value is NOT the buffer argument ***
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] string a string object to be copied
 * 				it can't be NULL
 * @param[in] stringFlags the flag to determine performing '.' --> '/'
 * @param[in] prependStr the string to be prepended before the string object to be copied
 * 				it can't be NULL but can be an empty string ""
 * @param[in] buffer the buffer for the string
 * @param[in] bufferLength the buffer length
 *
 * @return a char pointer to the string
 */
char*
copyStringToUTF8WithMemAlloc(J9VMThread *currentThread, j9object_t string, UDATA stringFlags, const char *prependStr, char *buffer, UDATA bufferLength);


/**
 * Copy certain number of Unicode characters from an offset into a Unicode character array to a UTF8 data buffer.
 *
 * @param[in] vmThread the current J9VMThread
 * @param[in] compressed if the Unicode character array is compressed
 * @param[in] nullTermination if the utf8 data is going to be NULL terminated
 * @param[in] stringFlags the flag to determine performing '.' --> '/'
 * @param[in] unicodeBytes a Unicode character array
 * @param[in] unicodeOffset an offset into the Unicode character array
 * @param[in] unicodeLength the number of Unicode characters to be copied
 * @param[in] utf8Data a utf8 data buffer
 * @param[in] utf8Length the size of the utf8 data buffer
 *
 * @return UDATA_MAX if a failure occurred, otherwise the number of utf8 data copied excluding null termination
 */
UDATA
copyCharsIntoUTF8Helper(
	J9VMThread *vmThread, BOOLEAN compressed, BOOLEAN nullTermination, UDATA stringFlags, j9object_t unicodeBytes, UDATA unicodeOffset, UDATA unicodeLength, U_8 *utf8Data, UDATA utf8Length);


/**
 * Copy a Unicode String to a UTF8 data buffer with NULL termination.
 *
 * @param[in] vmThread the current J9VMThread
 * @param[in] nullTermination if the utf8 data is going to be NULL terminated
 * @param[in] stringFlags the flag to determine performing '.' --> '/'
 * @param[in] utf8Data a utf8 data buffer
 * @param[in] utf8Length the size of the utf8 data buffer
 *
 * @return UDATA_MAX if a failure occurred, otherwise the number of utf8 data copied including null termination
 */
UDATA
copyStringToUTF8Helper(J9VMThread *vmThread, j9object_t string, BOOLEAN nullTermination, UDATA stringFlags, U_8 *utf8Data, UDATA utf8Length);


/**
 * !!! this method is for backwards compatibility with JIT usage !!!
 * Copy a Unicode String to a UTF8 data buffer with NULL termination.
 *
 * @param[in] vmThread the current J9VMThread
 * @param[in] string a string object to be copied, it can't be NULL
 * @param[in] stringFlags the flag to determine performing '.' --> '/'
 * @param[in] utf8Data a utf8 data buffer
 * @param[in] utf8Length the size of the utf8 data buffer
 *
 * @return 1 if a failure occurred, otherwise 0 for success
 */
UDATA
copyStringToUTF8(J9VMThread *vmThread, j9object_t string, UDATA stringFlags, U_8 *utf8Data, UDATA utf8Length);


/**
 * !!! this method is for backwards compatibility with JIT usage !!!
 * Copy a Unicode String to a UTF8 data buffer without NULL termination.
 * dest is assumed to have enough length - the easiest way to ensure this is to pass a buffer with 1.5 * string length in it
 *
 * @param[in] vmThread the current J9VMThread
 * @param[in] string a string object to be copied, it can't be NULL
 * @param[in] dest a utf8 data buffer
 *
 * @return UDATA_MAX if a failure occurred, otherwise the number of utf8 data copied excluding null termination
 */
UDATA
copyFromStringIntoUTF8(J9VMThread *vmThread, j9object_t string, char * dest);


/**
* @brief
* @param *vm
* @param *string
* @return IDATA
*/
IDATA
getStringUTF8Length(J9VMThread *vmThread,j9object_t string);


/**
* Check incoming class name characters and return following values accordingly:
* 	CLASSNAME_INVALID - if there is a character '/';
* 	CLASSNAME_VALID_NON_ARRARY - if it is valid and there is no '[' at beginning of class name string;
* 	CLASSNAME_VALID_ARRARY - if it is valid and there is a '[' at beginning of class name string.
*
* @param[in] *vmThread current thread
* @param[in] string the class name string
* @return a UDATA to indicate the nature of incoming class name string, see descriptions above.
*/
UDATA
verifyQualifiedName(J9VMThread *vmThread, j9object_t string);


/* ---------------- swalk.c ---------------- */

#if (!defined(J9VM_INTERP_STACKWALK_TRACING))
/**
* @brief
* @param currentThread
* @param walkState
* @return void
*/
void
freeStackWalkCaches(J9VMThread * currentThread, J9StackWalkState * walkState);
#endif /* J9VM_!INTERP_STACKWALK_TRACING */


#if (defined(J9VM_INTERP_STACKWALK_TRACING)  && !defined(J9VM_OUT_OF_PROCESS))
/**
* @brief
* @param walkState
* @param objectSlot
* @return void
*/
void
swMarkSlotAsObject(J9StackWalkState * walkState, j9object_t * objectSlot);
#endif /* J9VM_INTERP_STACKWALK_TRACING && !OUT_OF_PROCESS */


#if (defined(J9VM_INTERP_STACKWALK_TRACING))
/**
* @brief
* @param walkState
* @param level
* @param format
* @param ...
* @return void
*/
void
swPrintf(J9StackWalkState * walkState, UDATA level, char * format, ...);
#endif /* J9VM_INTERP_STACKWALK_TRACING */


#if (defined(J9VM_INTERP_STACKWALK_TRACING))
/**
* @brief
* @param walkState
* @return void
*/
void
swPrintMethod(J9StackWalkState * walkState);
#endif /* J9VM_INTERP_STACKWALK_TRACING */


#if (defined(J9VM_INTERP_STACKWALK_TRACING))
/**
* @brief
* @param walkState
* @param intSlot
* @param indirectSlot
* @param tag
* @return void
*/
void
swWalkIntSlot(J9StackWalkState * walkState, UDATA * intSlot, void * indirectSlot, void * tag);
#endif /* J9VM_INTERP_STACKWALK_TRACING */


#if (defined(J9VM_INTERP_STACKWALK_TRACING))
/**
* @brief
* @param walkState
* @param objectSlot
* @param indirectSlot
* @param tag
* @return void
*/
void
swWalkObjectSlot(J9StackWalkState * walkState, j9object_t * objectSlot, void * indirectSlot, void * tag);
#endif /* J9VM_INTERP_STACKWALK_TRACING */


/**
* @brief
* @param walkState
* @return UDATA
*/
UDATA
walkFrame(J9StackWalkState * walkState);


/**
* @brief
* @param *currentThread
* @param *walkState
* @return UDATA
*/
UDATA 
walkStackFrames(J9VMThread *currentThread, J9StackWalkState *walkState);


/**
* @brief Print and assert when an invalid reutrn address is detected in a JIT frame.
* @param *walkState
* @return void
*/
void
invalidJITReturnAddress(J9StackWalkState *walkState);


/**
 * Walk the stack slots (the pending pushes and locals) of a bytecoded frame.  The numberOfLocals
 * parameter must represent the true count, including the hidden locals for AI count, sync object or
 * receiver object for Object <init>.
 *
 * @param[in] *walkState current J9StackWalkState pointer
 * @param[in] *method method whose slots are being walked
 * @param[in] offsetPC index into the bytecode array
 * @param[in] *pendingBase highest-memory pending slot pointer
 * @param[in] pendingStackHeight number of pending slots to walk
 * @param[in] localBase highest-memory local slot pointer (e.g. arg0EA)
 * @param[in] numberOfLocals number of local slots (see above)
 * @param[in] alwaysLocalMap TRUE to force use of the local mapper, FALSE to use normal walkState logic
 */
void
walkBytecodeFrameSlots(J9StackWalkState *walkState, J9Method *method, UDATA offsetPC, UDATA *pendingBase, UDATA pendingStackHeight, UDATA *localBase, UDATA numberOfLocals, UDATA alwaysLocalMap);


/* ---------------- trace.c ---------------- */

#if (defined(J9VM_INTERP_TRACING)  || defined(J9VM_INTERP_UPDATE_VMCTRACING))  /* File Level Build Flags */

#if (defined(J9VM_INTERP_UPDATE_VMCTRACING))
/**
* @brief
* @param vmc
* @param writesVMThread
* @return void
*/
void
postHelperCall(J9VMThread * vmc, int writesVMThread);
#endif /* J9VM_INTERP_UPDATE_VMCTRACING */


#if (defined(J9VM_INTERP_UPDATE_VMCTRACING))
/**
* @brief
* @param vmc
* @param *sp
* @param *pc
* @param *literals
* @param *arg0EA
* @param readsVMC
* @param writesVMC
* @param *helperName
* @param *caller
* @return void
*/
void
preHelperCall(J9VMThread * vmc, void *sp, void *pc, void *literals, void *arg0EA, int readsVMC,
						  int writesVMC, char *helperName, char *caller);
#endif /* J9VM_INTERP_UPDATE_VMCTRACING */


/**
* @brief
* @param *vmStruct
* @return void
*/
void 
trace(J9VMThread *vmStruct);


#endif /* J9VM_INTERP_TRACING || INTERP_UPDATE_VMCTRACING */ /* End File Level Build Flags */


/* ---------------- vmaccess.c ---------------- */

/**
* @brief
* @param vmThread
* @return void
*/
void  
acquireExclusiveVMAccess(J9VMThread * vmThread);

/**
* @brief
* @param vmThread
* @return void
*/
void  
acquireSafePointVMAccess(J9VMThread * vmThread);


/**
* @brief
* @param vm
* @return void
*/
void
acquireExclusiveVMAccessFromExternalThread(J9JavaVM * vm);


/**
* @brief
* @param vmThread
* @param flags
* @return void
*/
void  
internalAcquireVMAccessClearStatus(J9VMThread * vmThread, UDATA flags);


/**
* @brief
* @param vmThread
* @return void
*/
void  
internalAcquireVMAccessNoMutex(J9VMThread * vmThread);


/**
* @brief Called when a JIT helper detects it does not have VM access.  Asserts and brings down the VM.
* @param vmThread
* @return void
*/
void 
mustHaveVMAccess(J9VMThread * vmThread);

/**
* @brief
* @param vmThread
* @param haltMask
* @return void
*/
void  
internalAcquireVMAccessNoMutexWithMask(J9VMThread * vmThread, UDATA haltMask);


/**
* @brief
* @param vmThread
* @param haltMask
* @return void
*/
void  
internalAcquireVMAccessWithMask(J9VMThread * vmThread, UDATA haltMask);


/**
* @brief
* @param vmThread
* @return void
*/
void 
internalReleaseVMAccessNoMutex(J9VMThread * vmThread);


/**
* @brief
* @param vmThread
* @param flags
* @return void
*/
void 
internalReleaseVMAccessSetStatus(J9VMThread * vmThread, UDATA flags);


/**
* @brief
* @param vmThread
* @return IDATA
*/
IDATA  
internalTryAcquireVMAccessNoMutex(J9VMThread * vmThread);


/**
* @brief
* @param vmThread
* @param haltMask
* @return IDATA
*/
IDATA  
internalTryAcquireVMAccessWithMask(J9VMThread * vmThread, UDATA haltMask);


/**
* @brief
* @param vmThread
* @return void
*/
void
releaseExclusiveVMAccess(J9VMThread * vmThread);

/**
* @brief
* @param vmThread
* @return void
*/
void
releaseSafePointVMAccess(J9VMThread * vmThread);


/**
* @brief
* @param vm
* @return void
*/
void
releaseExclusiveVMAccessFromExternalThread(J9JavaVM * vm);

/**
 * Enter a JNI critical region (i.e. GetPrimitiveArrayCritical or GetStringCritical).
 * Once a thread has successfully entered a critical region, it has privileges similar
 * to holding VM access. No object can move while any thread is in a critical region.
 *
 * @param vmThread  the J9VMThread requesting to enter a critical region
 */
void  
enterCriticalRegion(J9VMThread* vmThread);

/**
 * Enter a JNI critical region (i.e. GetPrimitiveArrayCritical or GetStringCritical).
 * Once a thread has successfully entered a critical region, it has privileges similar
 * to holding VM access. No object can move while any thread is in a critical region.
 *
 * @param vmThread  the J9VMThread requesting to exit a critical region
 */
void  
exitCriticalRegion(J9VMThread* vmThread);


#if (defined(J9VM_GC_REALTIME))
/**
* @brief initiates an exclusive access request for metronome
* @param vm
* @param block input parameter specifing whether caller should block if another request is ongoing
* @param responsesRequired the number of mutator threads that must voluntarily quiesce themselves
* @param gcPriority returned the new gc collector priority
* @return effectively a boolean the request is successful or not 
*/
UDATA
requestExclusiveVMAccessMetronome(J9JavaVM *vm, UDATA block, UDATA *responsesRequired, UDATA *gcPriority);

/**
* @brief initiates an exclusive access request for metronome
* @param vm
* @param block input parameter specifing whether caller should block if another request is ongoing
* @param vmResponsesRequired the number of mutator threads holding VM access that must voluntarily quiesce themselves
* @param jniResponsesRequired the number of mutator threads holding JNI critical access that must voluntarily quiesce themselves
* @param gcPriority returned the new gc collector priority
* @return effectively a boolean the request is successful or not
*/
UDATA
requestExclusiveVMAccessMetronomeTemp(J9JavaVM *vm, UDATA block, UDATA *vmResponsesRequired, UDATA *jniResponsesRequired, UDATA *gcPriority);

/**
* @brief waits for the responses to an exclusive access request by requestExclusiveVMAccessMetronome
* @param vm
* @param responsesRequired the number of mutator threads that must voluntarily quiesce themselves
* @return void
*/
void
waitForExclusiveVMAccessMetronome(J9VMThread * vmThread, UDATA responsesRequired);

/**
* @brief waits for the responses to an exclusive access request by requestExclusiveVMAccessMetronome
* @param vm
* @param vmResponsesRequired the number of mutator threads holding VM access that must voluntarily quiesce themselves
* @param jniResponsesRequired the number of mutator threads holding JNI critical access that must voluntarily quiesce themselves

* @return void
*/
void
waitForExclusiveVMAccessMetronomeTemp(J9VMThread * vmThread, UDATA vmResponsesRequired, UDATA jniResponsesRequired);

/**
* @brief releases the exclusive access
* @param vm
* @return void
*/
void
releaseExclusiveVMAccessMetronome(J9VMThread * vmThread);
#endif /* J9VM_GC_REALTIME */

/**
 * Release VM and/or JNI critical access. Record what was held in accessMask.
 * This will respond to any pending exclusive VM access request in progress.
 *
 * This function is intended for use with releaseAccess.
 *
 * @param vmThread  the J9VMThread requesting access
 * @param accessMask  the types of access to re-acquire
 */
void  
releaseAccess(J9VMThread* vmThread, UDATA* accessMask);

/**
 * Re-acquire VM and/or JNI critical access as specified by the accessMask.
 * This will block if another thread holds exclusive VM access.
 *
 * This function is intended for use with reacquireAccess.
 *
 * @param vmThread  the J9VMThread requesting access
 * @param accessMask  the types of access that were held
 */
void  
reacquireAccess(J9VMThread* vmThread, UDATA accessMask);

/* ---------------- vmbootlib.c ---------------- */

/**
* @brief
* @param vm
* @param libName
* @param handlePtr
* @param suppressError
* @return UDATA
*/
UDATA
openBootstrapLibrary(J9JavaVM* vm, char * libName, UDATA* handlePtr, UDATA suppressError);


/**
* @brief
* @param vmThread
* @param libName
* @param libraryPtr
* @param suppressError
* @return UDATA
*/
UDATA
registerBootstrapLibrary(J9VMThread * vmThread, const char * libName, J9NativeLibrary** handlePtr, UDATA suppressError);


/**
* @brief
* @param vmThread
* @param classLoader
* @param libName
* @param libraryPath
* @param libraryPtr
* @param errorBuffer
* @param bufferLength
* @return UDATA
*/
UDATA
registerNativeLibrary(J9VMThread * vmThread, J9ClassLoader * classLoader, const char * libName, char * libraryPath, J9NativeLibrary** libraryPtr, char* errorBuffer, UDATA bufferLength);


/**
 * Initialize native library callback functions to default values.
 * \param javaVM
 * \param library The library to initialize.
 * \return Zero on success, non-zero on failure.
 */
UDATA
initializeNativeLibrary(J9JavaVM * javaVM, J9NativeLibrary* library);


/* ---------------- vmhook.c ---------------- */

#if defined(J9VM_INTERP_NATIVE_SUPPORT)
/**
* @brief
* @param vm
* @return J9HookInterface**
*/
J9HookInterface**
getJITHookInterface(J9JavaVM* vm);
#endif /* INTERP_NATIVE_SUPPORT */


/**
* @brief
* @param vm
* @return J9HookInterface**
*/
J9HookInterface**
getVMHookInterface(J9JavaVM* vm);


/**
* @brief
* @param vm
* @return IDATA
*/
IDATA
initializeVMHookInterface(J9JavaVM* vm);


/**
* @brief
* @param vm
* @return void
*/
void
shutdownVMHookInterface(J9JavaVM* vm);

/* ---------------- vmphases.c ---------------- */

/**
* @brief Sets the phase in vm->phase and notifies other components that the phase has changed.
* @param vm
* @param phase
* @return void
*/
void
jvmPhaseChange(J9JavaVM* vm, UDATA phase);

/* ---------------- vmprops.c ---------------- */

/**
* @brief
* @param vm
* @return void
*/
void
freeSystemProperties(J9JavaVM * vm);


/**
* @brief
* @param vm
* @param name
* @param propertyPtr
* @return UDATA
*/
UDATA
getSystemProperty(J9JavaVM * vm, const char * name, J9VMSystemProperty ** propertyPtr);

/**
* @brief returns the J9VM version.
* @param vm
* @return const char*
 */
const char*
getJ9VMVersionString(J9JavaVM *vm);


/**
* @brief
* @param vm
* @return UDATA
*/
UDATA
initializeSystemProperties(J9JavaVM * vm);


/**
 * Assign a copy of value to the property.
* @param vm
 * @param property system property struct
 * @param value new value for the property
 * @return J9SYSPROP_ERROR_NONE on success
*/
UDATA
setSystemProperty(J9JavaVM * vm, J9VMSystemProperty * property, const char * value);


/**
 * Set the value of a system property to a new value.  Frees the old value if it used heap memory.
 * @param vm Java VM
 * @param property property to update
 * @param newValue the updated value of the property
 * @param allocated true if the value is in heap memory
 * @return J9SYSPROP_ERROR_NONE on success
 */
UDATA
setSystemPropertyValue(J9JavaVM * vm, J9VMSystemProperty * property, char * newValue, BOOLEAN allocated);
/**
 * Adds a new system property
 * @param vm
 * @param propName Null-terminated property name to be added.
 * @param propValue Null-terminated property name to be added.
 * @param flags
 * @return J9SYSPROP_ERROR_NONE on success, or a J9SYSPROP_ERROR_* value on failure.
 *
 * @note Does not check for duplicate entries, it is the caller responsibility to determine
 * if the system property exists prior to calling this function.
 */
UDATA
addSystemProperty(J9JavaVM * vm, const char* propName,  const char* propValue, UDATA flags);

/* ---------------- vmruntimestate.c ---------------- */

/**
 * Starts a new VM thread that listens for change in VM runtime state
 *
 * @param vm
 *
 * return 0 on success, -1 on error
 */
I_32
startVMRuntimeStateListener(J9JavaVM* vm);

/**
 * Stops VM runtime state listener thread
 *
 * @param vm
 *
 * @return void
 */
void
stopVMRuntimeStateListener(J9JavaVM *vm);

/**
 * Returns current VM runtime state
 *
 * @param vm
 *
 * @return either J9VM_RUNTIME_STATE_ACTIVE or J9VM_RUNTIME_STATE_IDLE
 *
 * @see updateVMRuntimeState
 */
U_32
getVMRuntimeState(J9JavaVM *vm);

/**
 * Sets VM runtime state to the given state
 *
 * @param vm
 * @param newState the new VM runtime state; valid values are J9VM_RUNTIME_STATE_ACTIVE or J9VM_RUNTIME_STATE_IDLE
 *
 * @return TRUE if state is successfully updated, FALSE otherwise
 *
 * @see getVMRuntimeState
 */
BOOLEAN
updateVMRuntimeState(J9JavaVM *vm, U_32 newState);

/**
 * Returns minimum time that the JVM waits in idle before setting its state as idle
 *
 * @param vm
 *
 * @return minimum idle wait time 
 */
U_32
getVMMinIdleWaitTime(J9JavaVM *vm);

/* ---------------- vmthinit.c ---------------- */

/**
* @brief
* @param *vm
* @return UDATA
*/
UDATA
initializeVMThreading(J9JavaVM *vm);

/**
* Frees memory allocated for a J9VMThread
* @param *vm J9JavaVM struct to access portLibrary
* @param *vmThread J9VMThread struct which will be freed
* @return void
*/
void
freeVMThread(J9JavaVM *vm, J9VMThread *vmThread);

/**
* @brief
* @param *vm
* @return void
*/
void
terminateVMThreading(J9JavaVM *vm);


/* ---------------- vmthread.c ---------------- */

/*
 * Perform thread setup before any java code is run on the thread.
 * Triggers J9HOOK_THREAD_ABOUT_TO_START.
 * 
 * @param currentThread the current J9VMThread
 */
void
threadAboutToStart(J9VMThread *currentThread);


/**
* @brief
* @param vm
* @param stackSize
* @param previousStack
* @return J9JavaStack *
*/
J9JavaStack *
allocateJavaStack(J9JavaVM * vm, UDATA stackSize, J9JavaStack * previousStack);


/**
* @brief
* @param vm
* @param stack
* @return void
*/
void
freeJavaStack(J9JavaVM * vm, J9JavaStack * stack);


/**
* @brief
* @param currentThread
* @return void
*/
void
fatalRecursiveStackOverflow(J9VMThread *currentThread);


/**
* @brief
* @param vm
* @param osThread
* @param privateFlags
* @param memorySpace
* @param threadObject the object associated with this thread, or NULL
* @return J9VMThread *
*/
J9VMThread *
allocateVMThread(J9JavaVM * vm, omrthread_t osThread, UDATA privateFlags, void * memorySpace, J9Object * threadObject);


/**
* @brief
* @param currentThread
* @param threadObject
* @return j9object_t
*/
j9object_t
createCachedOutOfMemoryError(J9VMThread * currentThread, j9object_t threadObject);


/**
* @brief
* @param vm
* @return J9VMThread*
*/
J9VMThread*
currentVMThread(J9JavaVM * vm);

/**
 * @brief
 * @param currentThread
 * @param pDeadlockedThreads
 * @param pBlockingObjects
 * @param flags
 * @return IDATA
 */
IDATA
findObjectDeadlockedThreads(J9VMThread *currentThread, j9object_t **pDeadlockedThreads, j9object_t **pBlockingObjects,
	UDATA flags);

#define J9VMTHREAD_FINDDEADLOCKFLAG_ALREADYHAVEEXCLUSIVE	0x1
#define J9VMTHREAD_FINDDEADLOCKFLAG_INCLUDESYNCHRONIZERS	0x2
#define J9VMTHREAD_FINDDEADLOCKFLAG_INCLUDEWAITING			0x4

/**
* @brief
* @param currentThread
* @param vmThread
* @return void
*/
void
haltThreadForInspection(J9VMThread * currentThread, J9VMThread * vmThread);


/**
* @brief
* @param *entryarg
* @return IDATA J9THREAD_PROC
*/
IDATA J9THREAD_PROC
javaThreadProc(void *entryarg);


#if (defined(J9VM_RAS_DUMP_AGENTS))  || (defined(J9VM_INTERP_SIG_QUIT_THREAD))
/**
* @brief
* @param *vm
* @param *self
* @param *toFile
* @return void
*/
void
printThreadInfo(J9JavaVM *vm, J9VMThread *self, char *toFile, BOOLEAN allThreads);
#endif /* J9VM_('RAS_DUMP_AGENTS' 'INTERP_SIG_QUIT_THREAD') */


/**
* @brief
* @param currentThread
* @param vmThread
* @return void
*/
void
resumeThreadForInspection(J9VMThread * currentThread, J9VMThread * vmThread);


/**
* @brief
* @param currentThread
* @param threadObject
* @param privateFlags
* @param osStackSize
* @param priority
* @param entryPoint
* @param entryArg
* @param schedulingParameters
* @return UDATA
*/
UDATA   
startJavaThread(J9VMThread * currentThread, j9object_t threadObject, UDATA privateFlags,
	UDATA osStackSize, UDATA priority, omrthread_entrypoint_t entryPoint, void * entryArg, j9object_t schedulingParameters);


/**
* @brief
* @param vmThread
* @param forkedByVM
* @return void
*/
void
threadCleanup(J9VMThread * vmThread, UDATA forkedByVM);


/**
* @brief
* @param vm
* @return void
*/
void OMRNORETURN
exitJavaThread(J9JavaVM * vm);


/**
 * @brief
 * @param vm
 * @param optArg
 * @return jint
 */
jint
threadParseArguments(J9JavaVM *vm, char *optArg);

/**
 * @brief  returns the Java priority for the thread specified.  For RealtimeThreads the priority comes
 *         from the priority field in PriorityParameters while for regular threads or any thread in a non-realtime vm it
 *         comes from the priority field in Thread
 *         Callers of this method must ensure that the Java Thread object for the thread is not NULL and
 *         that this cannot change while the call is being made
 *         This method should also only be called when we don't need barrier checks such as call from
 *         jvmti and ras
 *
 * @param vm the vm used to lookup the priority
 * @param thread the thread for which the priority should be returned
 * @return UDATA
 */
UDATA
getJavaThreadPriority(struct J9JavaVM *vm, J9VMThread* thread );


/* ---------------- hookableAsync.c ---------------- */

/**
* @brief Register a new async event handler.
* @param vm
* @param eventHandler
* @param userData
* @return IDATA
*
* Returns a non-negative handler key on success, or a negative number upon failure.
*/
IDATA
J9RegisterAsyncEvent(J9JavaVM * vm, J9AsyncEventHandler eventHandler, void * userData);

/**
* @brief Unregister an async event handler.
* @param vm
* @param handlerKey
* @return IDATA
*
* Returns 0 on success, non-zero on failure.
*/
IDATA
J9UnregisterAsyncEvent(J9JavaVM * vm, IDATA handlerKey);

/**
* @brief Signal an async event on a specific thread, or all threads if targetThread is NULL.
* @param vm
* @param targetThread
* @param handlerKey
* @return IDATA
*
* Returns 0 on success, non-zero on failure.
*/
IDATA
J9SignalAsyncEvent(J9JavaVM * vm, J9VMThread * targetThread, IDATA handlerKey);

/**
* @brief Signal an async event on a specific thread, or all threads if targetThread is NULL.
* @param vm
* @param targetThread
* @param handlerKey
* @return IDATA
*
* Returns 0 on success, non-zero on failure.
*/
IDATA
J9SignalAsyncEventWithoutInterrupt(J9JavaVM * vm, J9VMThread * targetThread, IDATA handlerKey);

/**
* @brief Cancel an async event on a specific thread, or all threads if targetThread is NULL.
* @param vm
* @param targetThread
* @param handlerKey
* @return IDATA
*
* Returns 0 on success, non-zero on failure.
*/
IDATA
J9CancelAsyncEvent(J9JavaVM * vm, J9VMThread * targetThread, IDATA handlerKey);

/**
* @brief Called from the async message handler once the async event flags have been fetched and zeroed.
* @param currentThread
* @param asyncEventFlags
*/
void
dispatchAsyncEvents(J9VMThread * currentThread, UDATA asyncEventFlags);

/* ---------------- findmethod.c ---------------- */

/**
* @brief Find the rom class in the segment that matches the PC.
* @param vmThread
* @param memorySegment
* @param methodPC
* @return J9ROMClass
*
* Returns the romclass, or NULL on failure.
*/
J9ROMClass * 
	findROMClassInSegment(J9VMThread *vmThread, J9MemorySegment *memorySegment, UDATA methodPC);

/**
* @brief Find the ROM method for the PC in a given ROM class.
* @param vmThread
* @param romClass
* @param methodPC
* @param offset
* @return J9ROMMethod
*
* Returns the method, or NULL on failure.
*/
J9ROMMethod * 
	findROMMethodInROMClass(J9VMThread *vmThread, J9ROMClass *romClass, UDATA methodPC, UDATA *offset);

/**
* @brief Finds the rom class given a PC.  Also returns the classloader it belongs to.
* @param vmThread
* @param methodPC
* @param resultClassLoader
* @return IDATA
*
* Returns the rom class, or NULL on failure.  resultClassLoader is filled in if non-null with the classloader associated.
*/
J9ROMClass * 
	findROMClassFromPC(J9VMThread *vmThread, UDATA methodPC, J9ClassLoader **resultClassLoader);

/* ---------------- ownedmonitors.c ---------------- */
typedef struct J9ObjectMonitorInfo {
	j9object_t object;
	IDATA depth;
	UDATA count;
} J9ObjectMonitorInfo;

/**
 * @brief Get the object monitors locked by a thread
 * @param currentThread
 * @param targetThread
 * @param info
 * @param infoLen
 * @return IDATA
 */
IDATA
getOwnedObjectMonitors(J9VMThread *currentThread, J9VMThread *targetThread,
		J9ObjectMonitorInfo *info, IDATA infoLen);

/**
 * @brief Construct a UTF8 illegalAccess message.
 *
 * @param[in] currentThread
 * @param[in] badMemberModifier
 * @param[in] senderClass
 * @param[in] targetClass
 * @param[in] errorType
 * @return char *
 */
char *
illegalAccessMessage(J9VMThread *currentThread, IDATA badMemberModifier, J9Class *senderClass, J9Class *targetClass, IDATA errorType);


/* ---------------- vtable.c ---------------- */

/**
 * @brief
 * @param vmStruct
 * @param vTableWriteCursor
 * @param currentMethod
 * @return void
 */
void
fillJITVTableSlot(J9VMThread *vmStruct, UDATA *currentSlot, J9Method *currentMethod);

/**
 * @brief
 * @param method
 * @return UDATA
 */
UDATA
getITableIndexForMethod(J9Method * method);

/**
 * @brief
 * @param method
 * @param clazz
 * @param vmThread
 * @return UDATA
 */
UDATA
getVTableIndexForMethod(J9Method * method, J9Class *clazz, J9VMThread *vmThread);

j9object_t
resolveMethodTypeRef(J9VMThread *vmThread, J9ConstantPool *constantPool, UDATA cpIndex, UDATA resolveFlags);

j9object_t
resolveMethodHandleRef(J9VMThread *vmThread, J9ConstantPool *constantPool, UDATA cpIndex, UDATA resolveFlags);

/* ---------------- logsupport.c ---------------- */

UDATA
queryLogOptions (J9JavaVM *vm, I_32 buffer_size, void *options, I_32 *data_size);

UDATA
setLogOptions (J9JavaVM *vm, char *options);

/* -------------------- MHInterpreter.cpp ------------ */

/**
* @brief
* @param currentThread
* @param methodType
* @return J9SFMethodTypeFrame The frame
*/
J9SFMethodTypeFrame * 
buildMethodTypeFrame(J9VMThread * currentThread, j9object_t methodType);

/* -------------------- drophelp.c ------------ */

/**
* @brief Set the SP to the unwindSP (as if we had stack walked) of the current frame.
* Keep literals consistent with that.
* @param curretThread
* @return UDATA TRUE or FALSE - is this a bytecoded frame (i.e. can't push on top of it)?
*/
UDATA  
dropPendingSendPushes(J9VMThread *currentThread);

/**
* @brief Prepare the current stack for throwing an exception (clear all pending, build necessary frames)
* @param curretThread
* @return void
*/
void  
prepareForExceptionThrow(J9VMThread *currentThread);

/**
 * Create a Java virtual machine
 * @param p_vm pointer to pointer to Java VM
 * @param p_env pointer to environment
 * @param createParams ully-resolved VM arguments, plus metadata, portLibrary, flags etc.
 *
 */
jint JNICALL J9_CreateJavaVM(JavaVM ** p_vm, void ** p_env, J9CreateJavaVMParams *createParams);
jint JNICALL J9_GetCreatedJavaVMs(JavaVM ** vm_buf, jsize bufLen, jsize * nVMs);
jint JNICALL J9_GetDefaultJavaVMInitArgs(void *);

/*
 * Used with J9_GetInterface() to specify which function table to return.
 */
typedef enum {
	IF_ZIPSUP
} J9_INTERFACE_SELECTOR;

/**
 * Returns an internal VM interface.
 * @param interfaceSelector select which interface to return
 * @param portLib port library
 * @param userData additional information required by the specific interface.
 * @return  function table
 */
void*
J9_GetInterface(J9_INTERFACE_SELECTOR interfaceSelector, J9PortLibrary* portLib, void *userData);

/* -------------------- BytecodeInterpreter.cpp ------------ */

/**
* @brief Execute the bytecode loop
* @param currentThread
* @return UDATA the action to take upon return to the builder interpreter
*/
UDATA  
bytecodeLoop(J9VMThread *currentThread);

/* -------------------- DebugBytecodeInterpreter.cpp ------------ */

/**
* @brief Execute the bytecode loop
* @param currentThread
* @return UDATA the action to take upon return to the builder interpreter
*/
UDATA  
debugBytecodeLoop(J9VMThread *currentThread);

/* -------------------- ClassInitialization.cpp ------------ */

/**
 * Enter the monitor for initializationLock, checking for immediate async and handling out of memory.
 *
 * @param[in] *currentThread current thread
 * @param[in] initializationLock the initialization lock object
 *
 * @return initialization lock object (possibly relocated), or NULL if an immediate async or OOM occurred
 */
j9object_t
enterInitializationLock(J9VMThread *currentThread, j9object_t initializationLock);

/**
 * Run the <clinit> for a class.
 *
 * @param[in] *currentThread current thread
 * @param[in] *j9clazz the J9Class to <clinit>
 */
void
initializeImpl(J9VMThread *currentThread, J9Class *j9clazz);

/**
 * Prepare a J9Class.
 *
 * @param[in] *currentThread current thread
 * @param[in] *clazz the J9Class to prepare
 */
void  
prepareClass(J9VMThread *currentThread, J9Class *clazz);

/**
 * Initialize a J9Class.
 *
 * @param[in] *currentThread current thread
 * @param[in] *clazz the J9Class to initialize
*/
void  
initializeClass(J9VMThread *currentThread, J9Class *clazz);

/* -------------------- threadpark.c ------------ */

void
threadParkImpl (J9VMThread* vmThread, IDATA timeoutIsEpochRelative, I_64 timeout);
void
threadUnparkImpl (J9VMThread* vmThread, j9object_t threadObject);

/* -------------------- threadhelp.cpp ------------ */

IDATA
monitorWaitImpl(J9VMThread *vmThread, j9object_t object, I_64 millis, I_32 nanos, UDATA interruptable);
IDATA
threadSleepImpl(J9VMThread* vmThread, I_64 millis, I_32 nanos);
omrthread_monitor_t
getMonitorForWait (J9VMThread* vmThread, j9object_t object);
/**
 * Helper function to create a thread with a specific thread category.
 *
 * @param[out] handle a pointer to a omrthread_t which will point to the thread (if successfully created)
 * @param[in] stacksize the size of the new thread's stack (bytes)<br>
 *			0 indicates use default size
 * @param[in] priority priorities range from J9THREAD_PRIORITY_MIN to J9THREAD_PRIORITY_MAX (inclusive)
 * @param[in] suspend set to non-zero to create the thread in a suspended state.
 * @param[in] entrypoint pointer to the function which the thread will run
 * @param[in] entryarg a value to pass to the entrypoint function
 * @param[in] category category of the thread to be created
 *
 * @return success or error code
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_xxx failure
 * 
 * @see omrthread_create
 */
IDATA
createThreadWithCategory(omrthread_t* handle, UDATA stacksize, UDATA priority, UDATA suspend,
	omrthread_entrypoint_t entrypoint, void* entryarg, U_32 category);

/**
 * Helper function to attach a thread with a specific category.
 *
 * If the OS thread is already attached, handle is set to point to the existing omrthread_t.
 * 
 * @param[out] handle a pointer to a omrthread_t which will point to the thread (if successfully attached)
 * @param[in]  category thread category
 *
 * @return success or error code
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_xxx failure
 * 
 * @see omrthread_attach
 */
IDATA
attachThreadWithCategory(omrthread_t* handle, U_32 category);

/* -------------------- J9OMRHelpers.cpp ------------ */

/**
 * Initialize an OMR VMThread.
 *
 * @param[in] *vm the J9JavaVM
 * @param[in] *vmThread the J9VMThread where the OMR VMThread is allocated.
 * @return a JNI error code
 */
jint initOMRVMThread(J9JavaVM *vm, J9VMThread *vmThread);

/**
 * Destroy an OMR VMThread.
 *
 * @param[in] *vm the J9JavaVM
 * @param[in] *vmThread the J9VMThread where the OMR VMThread is allocated.
 */
void destroyOMRVMThread(J9JavaVM *vm, J9VMThread *vmThread);

/**
 * Attach a J9VMThread to OMR.
 *
 * @param[in] *vm the J9JavaVM
 * @param[in] *vmThread the J9VMThread
 * @param[in] osThread the omrthread_t associated with the J9VMThread
 *
 * @return a JNI error code
 */
jint
attachVMThreadToOMR(J9JavaVM *vm, J9VMThread *vmThread, omrthread_t osThread);

/**
 * Detach a J9VMThread from OMR.
 *
 * @param[in] *vm the J9JavaVM
 * @param[in] *vmThread the J9VMThread
 */
void
detachVMThreadFromOMR(J9JavaVM *vm, J9VMThread *vmThread);

/**
 * Allocate a J9JavaVM followed by the OMR runtime and VM structures.
 *
 * @param[in] *portLibrary the port library
 *
 * @return newly-allocated J9JavaVM, or NULL on failure
 */
J9JavaVM *
allocateJavaVMWithOMR(J9PortLibrary *portLibrary);

/**
 * Attach the J9JavaVM to OMR.
 *
 * @param[in] *vm the J9JavaVM
 *
 * @return a JNI error code
 */
jint
attachVMToOMR(J9JavaVM *vm);

/**
 * Detach the J9JavaVM from OMR.
 *
 * @param[in] *vm the J9JavaVM
 */
void
detachVMFromOMR(J9JavaVM *vm);



/* ------------------- callin.cpp ----------------- */

/**
 * Convert and push arguments for a reflect method/constructor invocation.
 *
 * @param currentThread[in] the current J9VMThread
 * @param parameterTypes[in] the array of parameter types
 * @param arguments[in] the array of arguments
 *
 * @returns 0 on success, 1 for invalid argument count, 2 for illegal argument
 */
UDATA
pushReflectArguments(J9VMThread *currentThread, j9object_t parameterTypes, j9object_t arguments);

/* Thread library API accessible via GetEnv() */
#define J9THREAD_VERSION_1_1 0x7C010001

typedef struct J9ThreadEnv {
	uintptr_t 		(* get_priority)(omrthread_t thread);
	intptr_t 		(* set_priority)(omrthread_t thread, uintptr_t priority);

	omrthread_t 	(* self)(void);
	uintptr_t 		*(* global)(char *name);
	intptr_t 		(* attach)(omrthread_t *handle);
	intptr_t 		(* sleep)(int64_t millis);
	intptr_t 		(* create)(omrthread_t *handle, uintptr_t stacksize, uintptr_t priority, uintptr_t suspend, omrthread_entrypoint_t entrypoint, void *entryarg);
	void 		(* exit)(omrthread_monitor_t monitor);
	void 		(* abort)(omrthread_t handle);

	void 		(* priority_interrupt)(omrthread_t thread);
	uintptr_t		(* clear_interrupted)(void);

	intptr_t 		(* monitor_enter)(omrthread_monitor_t monitor);
	intptr_t 		(* monitor_exit)(omrthread_monitor_t monitor);
	intptr_t 		(* monitor_init_with_name)(omrthread_monitor_t *handle, uintptr_t flags, const char *name);
	intptr_t 		(* monitor_destroy)(omrthread_monitor_t monitor);
	intptr_t 		(* monitor_wait)(omrthread_monitor_t monitor);
	intptr_t 		(* monitor_notify_all)(omrthread_monitor_t monitor);

	void 		*(* tls_get)(omrthread_t thread, omrthread_tls_key_t key);
	intptr_t 		(* tls_set)(omrthread_t thread, omrthread_tls_key_t key, void *value);
	intptr_t 		(* tls_alloc)(omrthread_tls_key_t *handle);
	intptr_t 		(* tls_free)(omrthread_tls_key_t handle);

} J9ThreadEnv;

#ifdef __cplusplus
}
#endif

#endif /* vm_api_h */

