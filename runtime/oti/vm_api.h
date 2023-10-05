/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

#ifndef vm_api_h
#define vm_api_h

/**
* @file vm_api.h
* @brief Public API for the VM module.
*
* This file contains public function prototypes and
* type definitions for the VM module.
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
#define J9_CREATEJAVAVM_START_JITSERVER 16

#define HELPER_TYPE_MONITOR_WAIT_INTERRUPTABLE 1
#define HELPER_TYPE_MONITOR_WAIT_TIMED         2
#define HELPER_TYPE_THREAD_PARK                3
#define HELPER_TYPE_THREAD_SLEEP               4

typedef struct J9CreateJavaVMParams {
	UDATA j2seVersion;
	UDATA threadDllHandle;
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

/* ---------------- classsname.cpp ---------------- */

/**
 * Get the String representing the name of a Class. If the String has not been created
 * yet, create it and optionally intern and assign it to the Class.
 *
 * Current thread must have VM access and have a special frame on top of stack
 * with the J9VMThread roots up-to-date.
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] classObject the java/lang/Class being queried
 * @param[im] internAndAssign if true, intern the String and assign it to the Class object
 * @return the Class name String, or NULL on out of memory (exception will be pending)
 */
j9object_t
getClassNameString(J9VMThread *currentThread, j9object_t classObject, jboolean internAndAssign);

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
* @param currentThread
* @param classLoader
* @param className
* @param classNameLength
* @return UDATA
*/
J9Class*
peekClassHashTable(J9VMThread* currentThread, J9ClassLoader* classLoader, U_8* className, UDATA classNameLength);

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
 * @param allowedBitsForClassName the allowed bits for a valid class name,
 *        including CLASSNAME_INVALID, CLASSNAME_VALID_NON_ARRARY, CLASSNAME_VALID_ARRARY, or CLASSNAME_VALID.
 *
 * @return pointer to J9Class if success, NULL if fail
 *
 */
J9Class*
internalFindClassString(J9VMThread* currentThread, j9object_t moduleName, j9object_t className, J9ClassLoader* classLoader, UDATA options, UDATA allowedBitsForClassName);

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

/* ---------------- createramclass.c ---------------- */

/**
 * Use this if you don't have the class path index.
 */
#define J9_CP_INDEX_NONE -1
/**
 * Use this to peek the package ID table
 */
#define J9_CP_INDEX_PEEK -2

/**
 * Checks stack to see if element exists, if not adds the new element and returns TRUE. Otherwise, returns FALSE
 *
 * @param vmThread vmthread token
 * @param classloader loader associated with current element
 * @param clazz either romclass or ramclass
 * @param stack the loading or linking stack
 * @param comparator handle to function that will compare elements
 * @param maxStack maximum concurrent classloads or class linkage
 * @param stackpool pool for stack elements
 * @param throwException flag to indicate if exception should be thrown in the case of cirularity
 * @param ownsClassTableMutex flag to indicate if class table mutex is being held
 * @result TRUE is element exists in stack, FALSE otherwise
 */
BOOLEAN
verifyLoadingOrLinkingStack(J9VMThread *vmThread, J9ClassLoader *classLoader, void *clazz,
		J9StackElement **stack, BOOLEAN (*comparator)(void *, J9StackElement *), UDATA maxStack,
		J9Pool *stackpool, BOOLEAN throwException, BOOLEAN ownsClassTableMutex);

/**
 * Pops entry from stack
 *
 * @param vmThread vmthread token
 * @param stack the loading or linking stack
 * @param stackpool pool for stack elements
 */
void
popLoadingOrLinkingStack(J9VMThread *vmThread, J9StackElement **stack, J9Pool *stackpool);

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


#if defined(J9VM_OPT_CRIU_SUPPORT)

/* ---------------- CRIUHelpers.cpp ---------------- */
/**
 * @brief Queries if CRIU support is enabled. By default support
 * is not enabled, it can be enabled with `-XX:+EnableCRIUSupport`
 *
 * @param currentThread vmthread token
 * @return TRUE if enabled, FALSE otherwise
 */
BOOLEAN
isCRIUSupportEnabled(J9VMThread *currentThread);

/**
 * @brief Queries if CRIU support is enabled. By default support
 * is not enabled, it can be enabled with `-XX:+EnableCRIUSupport`
 *
 * @param vm javaVM token
 * @return TRUE if enabled, FALSE otherwise
 */
BOOLEAN
isCRIUSupportEnabled_VM(J9JavaVM *vm);

/**
 * @brief Queries if checkpointing is permitted. Note, when
 * -XX:+CRIURestoreNonPortableMode option is specified checkpointing
 * will not be permitted after the JVM has been restored from a checkpoint
 * (checkpoint once mode).
 *
 * @param currentThread vmthread token
 * @return TRUE if permitted, FALSE otherwise
 */
BOOLEAN
isCheckpointAllowed(J9VMThread *currentThread);

/**
 * @brief Queries if non-portable restore mode (specified via
 * -XX:+CRIURestoreNonPortableMode) is enabled. If so, checkpointing will
 * not be permitted after the JVM has been restored from a checkpoint
 * (checkpoint once mode).
 *
 * @param currentThread vmthread token
 * @return TRUE if enabled, FALSE otherwise
 */
BOOLEAN
isNonPortableRestoreMode(J9VMThread *currentThread);

/**
 * @brief JVM hooks to run before performing a JVM checkpoint
 *
 * @param currentThread vmthread token
 * @return TRUE on success, FALSE otherwise
 */
BOOLEAN
jvmCheckpointHooks(J9VMThread *currentThread);

/**
 * @brief JVM hooks to run after performing a JVM checkpoint. If
 * there is a fatal error, the process will terminate.
 *
 * @param currentThread vmthread token
 * @return TRUE on success, FALSE otherwise
 */
BOOLEAN
jvmRestoreHooks(J9VMThread *currentThread);

/**
 * @brief This iterates heap objects first, then goes through hook records,
 * and runs the checkpoint hook function.
 * ExclusiveVMAccess is required since J9InternalHookRecord holds live object references
 * and GC is not allowed while running these hook functions.
 *
 * @param[in] currentThread vmthread token
 * @param[in/out] nlsMsgFormat an NLS message
 *
 * @return BOOLEAN TRUE if no error, otherwise FALSE
 */
BOOLEAN
runInternalJVMCheckpointHooks(J9VMThread *currentThread, const char **nlsMsgFormat);

/**
 * @brief This runs the restore hook function, and cleanup.
 * ExclusiveVMAccess is required since J9InternalHookRecord hold live object references
 * and GC are not allowed while running these hook functions.
 *
 * @param[in] currentThread vmthread token
 * @param[in] isRestore If FALSE, run the hook specified for checkpoint, otherwise run the hook specified for restore
 * @param[in/out] nlsMsgFormat an NLS message
 *
 * @return BOOLEAN TRUE if no error, otherwise FALSE
 */
BOOLEAN
runInternalJVMRestoreHooks(J9VMThread *currentThread, const char **nlsMsgFormat);

/**
 * @brief This function runs the identity operations that were delayed
 * during the Java checkpoint and restore hooks.
 *
 * @param currentThread thread token
 * @return BOOLEAN TRUE if no error, otherwise FALSE
 */
BOOLEAN
runDelayedLockRelatedOperations(J9VMThread *currentThread);

/**
 * @brief This function delays the identity operations during the Java checkpoint and restore hooks.
 *
 * @param currentThread thread token
 * @param instance the identity object
 * @param operation the operation specified for the instance
 *
 * @return BOOLEAN TRUE if no error, otherwise FALSE
 */
BOOLEAN
delayedLockingOperation(J9VMThread *currentThread, j9object_t instance, UDATA operation);

/**
 * This adds an internal CRIU restore hook to be invoked for each class iterated
 * via allClassesStartDo/allClassesNextDo.
 *
 * @param[in] currentThread vmThread token
 * @param[in] hookFunc The hook function to be invoked for the hook record
 *
 * @return void
 */
void
addInternalJVMClassIterationRestoreHook(J9VMThread *currentThread, classIterationRestoreHookFunc hookFunc);

jobject
getRestoreSystemProperites(J9VMThread *currentThread);
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

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

/**
 * @brief Get a reference to jdk.internal.module.Modules.
 * @param *currentThread
 * @return class reference or null if error.
 */

jclass
getJimModules(J9VMThread *currentThread);
/* ---------------- description.c ---------------- */

/**
* @brief
* @param *ramClass
* @param *ramSuperClass
* @param *storage
* @return void
*/
void
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
calculateInstanceDescription( J9VMThread *vmThread, J9Class *ramClass, J9Class *ramSuperClass, UDATA *storage, J9ROMFieldOffsetWalkState *walkState, J9ROMFieldOffsetWalkResult *walkResult, BOOLEAN hasReferences);
#else /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */
calculateInstanceDescription( J9VMThread *vmThread, J9Class *ramClass, J9Class *ramSuperClass, UDATA *storage, J9ROMFieldOffsetWalkState *walkState, J9ROMFieldOffsetWalkResult *walkResult);
#endif /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */

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

/**
 * Update the fatalErrorStr field of the supplied J9VMDllLoadInfo object,
 * freeing as necessary any allocated string owned by that object.
 *
 * @param portLib the port library
 * @param info the J9VMDllLoadInfo object
 * @param error the new string (may be NULL)
 * @param errorIsAllocated indicates whether error (if not NULL) must eventually be freed
 */
void setErrorJ9dll(J9PortLibrary *portLib, J9VMDllLoadInfo *info, const char *error, BOOLEAN errorIsAllocated);


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
* @param bytecodeOffset
* @param romClass
* @param romMethod
* @param fileName
* @param lineNumber
* @param classLoader
* @param ramClass)
* @param userData
* @param pruneConstructors
* @param skipHiddenFrames
* @return UDATA
*/
UDATA
iterateStackTrace(J9VMThread * vmThread, j9object_t* exception,  UDATA  (*callback) (J9VMThread * vmThread, void * userData, UDATA bytecodeOffset, J9ROMClass * romClass, J9ROMMethod * romMethod, J9UTF8 * fileName, UDATA lineNumber, J9ClassLoader* classLoader, J9Class* ramClass), void * userData, UDATA pruneConstructors, UDATA skipHiddenFrames);


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
 * @param size
 * @return void
 */
void
setNegativeArraySizeException(J9VMThread *currentThread, I_32 size);

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
 * Prepare for throwing an exception. Find the exception class using its name.
 * Create an object using the exception class. Set an OutOfMemoryError if the
 * object cannot be created. Otherwise, set an exception pending using the
 * created object.
 *
 * Note this does not generate the "systhrow" dump event.
 *
 * @param vmThread[in] the current J9VMThread
 * @param exceptionClassName[in] the name of the exception class
 * @return void
 */
void
prepareExceptionUsingClassName(J9VMThread *vmThread, const char *exceptionClassName);

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

#if defined(J9VM_OPT_CRIU_SUPPORT)
/**
 * @brief Set JVMCRIUException to indicate that current operation is not allowed in CRIU single thread mode.
 * @param vmThread[in] the current J9VMThread
 * @param moduleName[in] the NLS module
 * @param messageNumber[in] the NLS message
 *
 * @return void
 */
void
setCRIUSingleThreadModeJVMCRIUException(J9VMThread *vmThread, U_32 moduleName, U_32 messageNumber);
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

/* ---------------- extendedMessageNPE.cpp ---------------- */

/**
 * Return an extended NPE message.
 *
 * Note: the caller is responsible for freeing the returned string if it is not NULL.
 *
 * @param npeMsgData - the J9NPEMessageData structure holding romClass/romMethod/npePC
 * @return char* An extended NPE message or NULL if such a message can't be generated
 */
char*
getNPEMessage(J9NPEMessageData *npeMsgData);

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
* @brief Determine if a JNI ref is an internal class ref
* @param *vm
* @param ref
* @return UDATA
*/
UDATA
jniIsInternalClassRef(J9JavaVM *vm, jobject ref);

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
* @param size
* @return void
*/
void JNICALL
gpCheckSetNegativeArraySizeException(J9VMThread* env, I_32 size);


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
freeClassLoaderEntries(J9VMThread * vmThread, J9ClassPathEntry **entries, UDATA count, UDATA initCount);

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
 * @brief Queries whether valueTypes are enable on the JVM
 * @param vm A handle to the J9JavaVM
 * @return TRUE if valueTypes are enabled, FALSE otherwise
 */
BOOLEAN
areValueTypesEnabled(J9JavaVM *vm);

/**
 * @brief Queries whether flattenable valueTypes are enable on the JVM
 * @param vm A handle to the J9JavaVM
 * @return TRUE if flattenable valueTypes are enabled, FALSE otherwise
 */
BOOLEAN
areFlattenableValueTypesEnabled(J9JavaVM *vm);

/**
 * Checks if args in specified args array have been consumed by the JVM, if not it outputs a warning message
 * and returns FALSE. This function consults the -XXvm:ignoreUnrecognized, -XX:[+|-]IgnoreUnrecognizedVMOptions
 * and -XX:[-|+]IgnoreUnrecognizedXXColonOptions to determine if args are consumed or not.
 *
 * @param vm vm token
 * @param portLibrary port lib token
 * @param j9vm_args the VM args array
 * @return TRUE if all args are consumed, FALSE otherwise
 */
UDATA
checkArgsConsumed(J9JavaVM * vm, J9PortLibrary* portLibrary, J9VMInitArgs* j9vm_args);

/**
 * @brief Queries if -XX:DiagnoseSyncOnValueBasedClasses=1 or -XX:DiagnoseSyncOnValueBasedClasses=2 are found in the CML
 * @param[in] vm pointer to the J9JavaVM
 * @return FALSE if neither of -XX:DiagnoseSyncOnValueBasedClasses=1/-XX:DiagnoseSyncOnValueBasedClasses=2 are found in the CML.
 * 			(i.e. neither J9_EXTENDED_RUNTIME2_VALUE_BASED_EXCEPTION/J9_EXTENDED_RUNTIME2_VALUE_BASED_WARNING are set in
 * 			vm->extendedRuntimeFlags2)
 * 			Otherwise, return TRUE.
 */
BOOLEAN areValueBasedMonitorChecksEnabled(J9JavaVM *vm);

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
 * Initializes class path entries. Path of each class path entry is set using the 'classPath' provided as parameter.
 * Each path in 'classPath' is separated by 'classPathSeparator'.
 * If 'initClassPathEntry' is TRUE, then each class path entry in initialized by calling 'initializeClassPathEntry()' function.
 *
 * @param[in] vm pointer to the J9JavaVM
 * @param[in] classPath set of paths separated by 'classPathSeparator'
 * @param[in] classPathSeparator platform specific class path separator
 * @param[in] cpFlags clas path entry flags
 * @param[in] initClassPathEntry if TRUE initialize each class path entry
 * @param[out] classPathEntries returns the pointer array of class path entries.
 *
 * @return number of class path entries initialized
 */
UDATA
initializeClassPath(J9JavaVM *vm, char *classPath, U_8 classPathSeparator, U_16 cpFlags, BOOLEAN initClassPathEntry, J9ClassPathEntry ***classPathEntries);

/*
 * Initialize the given class path entry. This involves determining what kind
 * of entry it is, and preparing to do class loads (without actually doing any).
 *
 * @param[in] javaVM pointer to the J9JavaVM
 * @param[in] cpEntry pointer to J9ClassPathEntry to be initialized
 *
 * @return IDATA type of the entry which is one of the following
 * 		CPE_TYPE_DIRECTORY if it's a directory
 * 		CPE_TYPE_JAR if it's a ZIP file, extraInfo contains the J9ZipFile
 * 		CPE_TYPE_JIMAGE if it's a jimage file
 * 		CPE_TYPE_USUSABLE if it's a bad entry, don't try to use it anymore
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

/**
 * @brief Register jvminit.c::predefinedHandlerWrapper using j9sig_set_*async_signal_handler
 * for the specified signal
 *
 * @param[in] vm pointer to a J9JavaVM
 * @param[in] signal integer value of the signal
 * @param[out] oldOSHandler points to the old signal handler function
 *
 * @return 0 on success and non-zero on failure
 */
IDATA
registerPredefinedHandler(J9JavaVM *vm, U_32 signal, void **oldOSHandler);

/**
 * @brief Register a signal handler function with the OS for the specified signal.
 *
 * @param[in] vm pointer to a J9JavaVM
 * @param[in] signal integer value of the signal
 * @param[in] newOSHandler address to the new signal handler function which will be registered
 * @param[out] oldOSHandler points to the old signal handler function
 *
 * @return 0 on success and non-zero on failure
 */
IDATA
registerOSHandler(J9JavaVM *vm, U_32 signal, void *newOSHandler, void **oldOSHandler);

#if defined(J9VM_OPT_JITSERVER)
/**
 * @brief checks if the runtime flag for enabling JITServer is set or not
 *
 * @param[in] vm pointer to a J9JavaVM
 *
 * @return TRUE if the flag for JITServer is set, FALSE otherwise
 */
BOOLEAN
isJITServerEnabled(J9JavaVM *vm);

#endif /* J9VM_OPT_JITSERVER */

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

/* Flag(s) used by hashClassTableStartDo()/hashClassTableNextDo() and set to J9HashTableState.flags
 * to control whether to skip special classes, like hidden classes, in the hash table iteration.
 */
#define J9_HASH_TABLE_STATE_FLAG_SKIP_HIDDEN 1

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
* @param *classLoader The class loader to use.
* @param *walkState The J9HashTableState.
* @param flags Flags to control whether to skip special classes, like hidden classes, in the iteration.
* @return J9Class*
*/
J9Class*
hashClassTableStartDo(J9ClassLoader *classLoader, J9HashTableState* walkState, UDATA flags);

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
 * @brief Locates and returns a structure containing load location for the given class
 * Caller must acquire classLoaderModuleAndLocationMutex before making the call
 *
 * @param currentThread current thread pointer
 * @param clazz J9Class for which load location is to be searched
 *
 * @return pointer to J9ClassLocation for the given class, or NULL if not found
 */
J9ClassLocation *
findClassLocationForClass(J9VMThread *currentThread, J9Class *clazz);

/* ---------------- ModularityHashTables.c ---------------- */

/**
 * Used by classLoader->moduleHashTable which doesn't allow multiple modules with same module name.
 * Create a new J9HashTable with hashFn (moduleNameHashFn) and hashEqualFn (moduleNameHashEqualFn).
 * Using module name as the key can determine if two modules are same based on their module names.
 *
 * @param javaVM A java VM
 * @param initialSize initial size
 * @return an initialized J9HashTable on success, otherwise NULL.
 */
J9HashTable *
hashModuleNameTableNew(J9JavaVM *javaVM, U_32 initialSize);

/**
 * Used by J9Package->exportsHashTable, J9Module->readAccessHashTable, and J9Module->removeAccessHashTable
 * which might contain modules loaded by different classloader but with same module names.
 * Create a new J9HashTable with hashFn (modulePointerHashFn) and hashEqualFn (modulePointerHashEqualFn).
 * Using J9Module pointer as the key can differentiate modules loaded by different classloader with same module name.
 * @param javaVM A java VM
 * @param initialSize initial size
 * @return an initialized J9HashTable on success, otherwise NULL.
 */
J9HashTable *
hashModulePointerTableNew(J9JavaVM *javaVM, U_32 initialSize);

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
UDATA
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


/* ---------------- PackageIDHashTable.c ---------------- */

/**
* @brief Find the package ID for the given name and length
* @note Must own the classTableMutex.
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
* @note Must own the classTableMutex.
* @note The ROM class may be a dummy with only the package name.
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

/**
* @brief
* @param *vm
* @return IDATA
*/
IDATA
configureRasDump(J9JavaVM *vm);


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
 * Report a hot field if the JIT has determined that the field has met appropriate thresholds to be determined a hot field.
 * Valid if dynamicBreadthFirstScanOrdering is enabled.
 *
 * @param vm[in] pointer to the J9JavaVM
 * @param reducedCpuUtil normalized cpu utilization of the hot field for the method being compiled
 * @param clazz pointer to the class where a hot field should be added
 * @param fieldOffset value of the field offset that should be added as a hot field for the given class
 * @param reducedFrequency normalized block frequency of the hot field for the method being compiled
 */
void
reportHotField(J9JavaVM *javaVM, int32_t reducedCpuUtil, J9Class* clazz, uint8_t fieldOffset,  uint32_t reducedFrequency);

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
* @param vm[in]					pointer to the J9JavaVM
* @param romClass[in]			the ROM class whose fields will be iterated
* @param superClazz[in]			the RAM super class of the class whose fields will be iterated
* @param state[in/out]			the walk state that can subsequently be passed to fieldOffsetsNextDo()
* @param flags[in]				J9VM_FIELD_OFFSET_WALK_* flags
* @param flattenedClassCache[in]	A table of all flattened instance field types
* @return J9ROMFieldOffsetWalkResult *
*/
J9ROMFieldOffsetWalkResult *
#ifdef J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES
fieldOffsetsStartDo(J9JavaVM *vm, J9ROMClass *romClass, J9Class *superClazz, J9ROMFieldOffsetWalkState *state, U_32 flags, J9FlattenedClassCache *flattenedClassCache);
#else /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */
fieldOffsetsStartDo(J9JavaVM *vm, J9ROMClass *romClass, J9Class *superClazz, J9ROMFieldOffsetWalkState *state, U_32 flags);
#endif /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */

/**
 * Initialize fields offsets into FCC
 *
 * @param[in] *currentThread the current thread
 * @param[in] *clazz class
 *
 * @returns void
 */
void
calculateFlattenedFieldAddresses(J9VMThread *currentThread, J9Class *clazz);

/**
 * Initialize fields to default values when the class
 * contains unflattened flattenables.
 *
 * @param[in] *currentThread the current thread
 * @param[in] *clazz the resolved class
 * @param[in] instance J9Object with unflattened flattenable fields
 *
 * @returns void
 */
void
defaultValueWithUnflattenedFlattenables(J9VMThread *currentThread, J9Class *clazz, j9object_t instance);

/**
 * Initialize static fields to default values when the class
 * contains flattenable statics. Currently none of the static fields are flattened.
 *
 * @param[in] currentThread the current thread
 * @param[in] clazz the class being loaded
 * @param[in] entry The FCC entry for the static field
 * @param[in] entryClazz the clazz in the FCC entry for the static field
 *
 * @returns void
 */
void
classPrepareWithWithUnflattenedFlattenables(J9VMThread *currentThread, J9Class *clazz, J9FlattenedClassCacheEntry *entry, J9Class *entryClazz);

/**
 * Compare two objects for equality. This helper will perform a
 * structural comparison if both objecst are valueTypes
 *
 * @param[in] currentThread the current thread
 * @param[in] lhs first operand
 * @param[in] rhs second operand
 *
 * @return TRUE if both objects are equal, false otherwise
 */
BOOLEAN
valueTypeCapableAcmp(J9VMThread *currentThread, j9object_t lhs, j9object_t rhs);

/**
 * Determines if a name or a signature pointed by a J9UTF8 pointer is a Qtype.
 *
 * @param[in] utfWrapper J9UTF8 pointer that points to the name or the signature
 *
 * @return TRUE if the name or the signature pointed by the J9UTF8 pointer is a Qtype, FALSE otherwise
 */
BOOLEAN
isNameOrSignatureQtype(J9UTF8 *utfWrapper);


/**
 * Determines if the classref c=signature is a Qtype. There is no validation performed
 * to ensure that the cpIndex points at a classref.
 *
 * @param[in] cpContextClass ramClass that owns the constantpool being queried
 * @param[in] cpIndex the CP index
 *
 * @return TRUE if classref is a Qtype, FALSE otherwise
 */
BOOLEAN
isClassRefQtype(J9Class *cpContextClass, U_16 cpIndex);

/**
 * Determines if null restricted attribute is set on a field or not.
 *
 * @param[in] field The field to be checked
 *
 * @return TRUE if the field has null restricted attribute set, FALSE otherwise
 */
BOOLEAN
isFieldNullRestricted(J9ROMFieldShape *field);

/**
 * Performs an aaload operation on an object. Handles flattened and non-flattened cases.
 *
 * Assumes recieverObject is not null.
 * All AIOB exceptions must be thrown before calling.
 *
 * Returns null if newObjectRef retrieval fails.
 *
 * If fast == false, special stack frame must be built and receiverObject must be pushed onto it.
 *
 * @param[in] currentThread thread token
 * @param[in] receiverObject arrayObject
 * @param[in] index array index
 * @param[in] fast bool for fast or slow path
 *
 * @return array element
 */
j9object_t
loadFlattenableArrayElement(J9VMThread *currentThread, j9object_t receiverObject, U_32 index, BOOLEAN fast);

/**
 * Performs an aastore operation on an object. Handles flattened and non-flattened cases.
 *
 * Assumes recieverObject and paramObject are not null.
 * All AIOB exceptions must be thrown before calling.
 *
 * @param[in] currentThread thread token
 * @param[in] receiverObject arrayObject
 * @param[in] index array index
 * @param[in] paramObject obj arg
 */
void
storeFlattenableArrayElement(J9VMThread *currentThread, j9object_t receiverObject, U_32 index, j9object_t paramObject);

/**
* @brief Iterate over fields of the specified class in JVMTI order.
* @param state[in/out]  the walk state that was initialized via fieldOffsetsStartDo()
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
* @param state[in/out]  the walk state that was initialized via fullTraversalFieldOffsetsStartDo()
* @return J9ROMFieldShape *
*/
J9ROMFieldShape *
fullTraversalFieldOffsetsNextDo(J9ROMFullTraversalFieldOffsetWalkState *state);

#ifdef J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES
/**
 * @brief Search for ramClass in flattened class cache
 *
 * @param flattenedClassCache[in]	A table of flattened instance field types
 * @param className[in]				Name of class to search
 * @param classNameLength[in]		Length of class name to search
 *
 * @return J9Class if found NULL otherwise
 */
J9Class *
findJ9ClassInFlattenedClassCache(J9FlattenedClassCache *flattenedClassCache, U_8 *className, UDATA classNameLength);

/**
 * @brief Search for index of field in flattened class cache
 *
 * @param flattenedClassCache[in]	A table of flattened instance field types
 * @param nameAndSignature[in]		The name and signature of field to look for
 *
 * @return index if found, UDATA_MAX otherwise
 */
UDATA
findIndexInFlattenedClassCache(J9FlattenedClassCache *flattenedClassCache, J9ROMNameAndSignature *nameAndSignature);
#endif /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */

/**
 * Returns the offset of a qtype field.
 *
 * @param[in] fieldOwner the J9class that defines the field
 * @param[in] field romfieldshape of the field
 *
 * @return field offset
 */
UDATA
getFlattenableFieldOffset(J9Class *fieldOwner, J9ROMFieldShape *field);

/**
 * Returns if a field is flattened. `J9_IS_J9CLASS_FLATTENED` will be deprecated.
 * This helper assumes field is a qtype.
 *
 * @param[in] fieldOwner the J9class that defines the field
 * @param[in] field romfieldshape of the field
 *
 * @return TRUE if field is flattened, false otherwise
 */
BOOLEAN
isFlattenableFieldFlattened(J9Class *fieldOwner, J9ROMFieldShape *field);

/**
 * Returns the type of an instance field. `J9_IS_J9CLASS_FLATTENED` will be deprecated.
 * This helper assumes field is a qtype.
 *
 * @param[in] fieldOwner the J9class that defines the field
 * @param[in] field romfieldshape of the field
 *
 * @return TRUE if field is flattened, false otherwise
 */
J9Class *
getFlattenableFieldType(J9Class *fieldOwner, J9ROMFieldShape *field);

/**
 * Returns the size of an instance field. `J9_VALUETYPE_FLATTENED_SIZE` will be deprecated.
 * This helper assumes field is a qtype.
 *
 * @param[in] currentThread thread token
 * @param[in] fieldOwner the J9class that defines the field
 * @param[in] fieldref cp ref of the field
 *
 * @return TRUE if field is flattened, false otherwise
 */
UDATA
getFlattenableFieldSize(J9VMThread *currentThread, J9Class *fieldOwner, J9ROMFieldShape *field);

/**
 * Returns the size of an array element field. `J9_VALUETYPE_FLATTENED_SIZE`
 * will be deprecated.
 *
 * @param[in] arrayClass containing elements
 *
 * @return size of the array element
 */
UDATA
arrayElementSize(J9ArrayClass* arrayClass);

/**
 * Performs a getfield operation on an object. Handles flattened and non-flattened cases.
 * This helper assumes that the cpIndex points to the fieldRef of a resolved Qtype. This helper
 * also assumes that the cpIndex points to an instance field.
 *
 * @param currentThread thread token
 * @param cpEntry the RAM cpEntry for the field, needs to be resolved
 * @param receiver receiver object
 * @param fastPath performs fastpath allocation, no GC. If this is false
 * 			frame must be built before calling as GC may occur
 *
 * @return NULL if allocation fails, valuetype otherwise
 */
j9object_t
getFlattenableField(J9VMThread *currentThread, J9RAMFieldRef *cpEntry, j9object_t receiver, BOOLEAN fastPath);

/**
 * Performs a clone operation on an object.
 *
 * @param currentThread thread token
 * @param receiverClass j9class of original object
 * @param original object to be cloned
 * @param fastPath performs fastpath allocation, no GC. If this is false
 * 			frame must be built before calling as GC may occur
 *
 * @return NULL if allocation fails, valuetype otherwise
 */
j9object_t
cloneValueType(J9VMThread *currentThread, J9Class *receiverClass, j9object_t original, BOOLEAN fastPath);

/**
 * Performs a putfield operation on an object. Handles flattened and non-flattened cases.
 * This helper assumes that the cpIndex points to the fieldRef of a resolved Qtype. This helper
 * also assumes that the cpIndex points to an instance field.
 *
 * @param currentThread thread token
 * @param cpEntry the RAM cpEntry for the field, needs to be resolved
 * @param receiver receiver object
 * @param paramObject parameter object
 */
void
putFlattenableField(J9VMThread *currentThread, J9RAMFieldRef *cpEntry, j9object_t receiver, j9object_t paramObject);

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
 *@brief find field in class
 *@param vmstruct vmthread token
 *@param clazz ramclass
 *@param fieldName name of field
 *@param fieldNameLength length of field name
 *@param signature field signature
 *@param signatureLength length of field name signature
 *@param definingClass class that defines the field
 *@param offsetAddress instance field offset or static field address
 *@param options lookup options
 *@return romfieldShape if field is found
 */
J9ROMFieldShape*
findFieldExt(J9VMThread *vmStruct, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, J9Class **definingClass, UDATA *offsetOrAddress, UDATA options);

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
 * Determine if the targetClass is subject to the package access check.
 * Assuming targetClass is NOT NULL.
 *
 * @param *vm[in] the J9JavaVM
 * @param srcClassLoader[in] Current class loader creating RAM class
 * @param srcModule[in] The module where current RAM class is loaded from
 * @param targetClass[in] The RAM class being examined to be determined if a package access check should be performed
 * @return TRUE if a package access check is required, otherwise FALSE.
 */
BOOLEAN
requirePackageAccessCheck(J9JavaVM *vm, J9ClassLoader *srcClassLoader, J9Module *srcModule, J9Class *targetClass);

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
* @brief Iterate through the segmentList and pass the J9MemorySegment * to the segmentCallback
* @param segmentList The J9MemorySegmentList to iterate through.  Must not be NULL
* @param segmentCallBack The user supplied callback that operates on the segment.  Must not be NULL.
* @param userData A void* passed through to the callback for tracking state
*/
void
allSegmentsInMemorySegmentListDo(J9MemorySegmentList *segmentList, void (* segmentCallback)(J9MemorySegment*, void*), void *userData);

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

/* ---------------- stringhelpers.cpp ---------------- */

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
 * Copy a string object to a UTF8 data buffer, and optionally prepend a string before it.
 *
 * @note The caller must free the memory from this pointer if the return value is NOT the buffer argument.
 * @note If the buffer is not large enough to encode the string this function will allocate enough memory to encode
 *       the worst-case UTF8 encoding of the supplied UTF16 string (length of string * 3 bytes).
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] string a string object to be copied
 * 				it can't be NULL
 * @param[in] stringFlags the flag to determine performing '.' --> '/' or NULL termination
 * @param[in] prependStr the string to be prepended before the string object to be copied
 * 				it can't be NULL but can be an empty string ""
 * @param[in] prependStrLength The length of prependStr as computed by strlen.
 * @param[in] buffer the buffer for the string
 * @param[in] bufferLength the buffer length
 * @param[out] utf8Length If not NULL returns the computed length (in bytes) of the copied UTF8 string in the buffer excluding the NULL terminator.
 *
 * @return a char pointer to the string
 */
char*
copyStringToUTF8WithMemAlloc(J9VMThread *vmThread, j9object_t string, UDATA stringFlags, const char *prependStr, UDATA prependStrLength, char *buffer, UDATA bufferLength, UDATA *utf8Length);


/**
* Copy a string object to a J9UTF8 data buffer, and optionally prepend a string before it.
*
* @note The caller must free the memory from this pointer if the return value is NOT the buffer argument.
* @note If the buffer is not large enough to encode the string this function will allocate enough memory to encode
*       the worst-case UTF8 encoding of the supplied UTF16 string (length of string * 3 bytes).
*
* @param[in] currentThread the current J9VMThread
* @param[in] string a string object to be copied
* 				it can't be NULL
* @param[in] stringFlags the flag to determine performing '.' --> '/' or NULL termination
* @param[in] prependStr the string to be prepended before the string object to be copied
* 				it can't be NULL but can be an empty string ""
* @param[in] prependStrLength The length of prependStr as computed by strlen.
* @param[in] buffer the buffer for the string
* @param[in] bufferLength the buffer length
*
* @return a J9UTF8 pointer to the string
*/
J9UTF8*
copyStringToJ9UTF8WithMemAlloc(J9VMThread *vmThread, j9object_t string, UDATA stringFlags, const char *prependStr, UDATA prependStrLength, char *buffer, UDATA bufferLength);


/**
 * Copy a Unicode String to a UTF8 data buffer.
 *
 * @param[in] vmThread the current J9VMThread
 * @param[in] string a string object to be copied, it can't be NULL
 * @param[in] stringFlags the flag to determine performing '.' --> '/' or NULL termination
 * @param[in] stringOffset the offset into \p string at which to start the copy
 * @param[in] stringLength the number of \p string characters to copy
 * @param[in] utf8Data a utf8 data buffer
 * @param[in] utf8DataLength the length of utf8Data in number of bytes
 *
 * @return The computed length (in bytes) of the copied UTF8 string in the buffer excluding any NULL terminator.
 */
UDATA
copyStringToUTF8Helper(J9VMThread *vmThread, j9object_t string, UDATA stringFlags, UDATA stringOffset, UDATA stringLength, U_8 *utf8Data, UDATA utf8DataLength);


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
* @param[in] className the class name string
* @param[in] classNameLength the length of the class name string
* @param[in] allowedBitsForClassName the allowed bits for a valid class name,
*            including CLASSNAME_VALID_NON_ARRARY, CLASSNAME_VALID_ARRARY, or CLASSNAME_VALID.
*
* @return a UDATA to indicate the nature of incoming class name string, see descriptions above.
*/
UDATA
verifyQualifiedName(J9VMThread *vmThread, U_8 *className, UDATA classNameLength, UDATA allowedBitsForClassName);


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


#if defined(J9VM_INTERP_STACKWALK_TRACING)
/**
* @brief
* @param walkState
* @param objectSlot
* @return void
*/
void
swMarkSlotAsObject(J9StackWalkState * walkState, j9object_t * objectSlot);
#endif /* J9VM_INTERP_STACKWALK_TRACING */


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
* @brief Print and assert when an invalid return address is detected in a JIT frame.
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

/* ---------------- visible.c ---------------- */
#if JAVA_SPEC_VERSION >= 11
/**
 * Loads the nest host and ensures that the nest host belongs
 * to the same runtime package as the class. This function requires
 * VMAccess
 *
 * @param[in] vmThread handle to vmThread
 * @param[in] clazz the class to search the nest host
 * @param[in] options lookup options
 * @param[out] nestHostFound the nest host of clazz
 *
 * @return 	J9_VISIBILITY_ALLOWED if success,
 * 			J9_VISIBILITY_NEST_HOST_LOADING_FAILURE_ERROR if failed to load nesthost,
 * 			J9_VISIBILITY_NEST_HOST_DIFFERENT_PACKAGE_ERROR if nesthost is not in the same runtime package
 */
UDATA
loadAndVerifyNestHost(J9VMThread *vmThread, J9Class *clazz, UDATA options, J9Class **nestHostFound);

/**
 * Sets the nestmates error based on the errorCode
 *
 * @param vmThread vmthread token
 * @param nestMember the j9lass requesting the nesthost
 * @param nestHost the actual nest host, this may be NULL
 * @param errorCode the error code representing the exception to throw
 * 	J9_VISIBILITY_NEST_HOST_LOADING_FAILURE_ERROR
 * 	J9_VISIBILITY_NEST_HOST_DIFFERENT_PACKAGE_ERROR
 * 	J9_VISIBILITY_NEST_MEMBER_NOT_CLAIMED_ERROR
 */
void
setNestmatesError(J9VMThread *vmThread, J9Class *nestMember, J9Class *nestHost, IDATA errorCode);
#endif /* JAVA_SPEC_VERSION >= 11 */

/* ---------------- VMAccess.cpp ---------------- */

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
* @param currentThread
* @param vmThread
* @return void
*/
void
resumeThreadForInspection(J9VMThread * currentThread, J9VMThread * vmThread);


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


#if (defined(J9VM_GC_REALTIME))
/**
* @brief initiates an exclusive access request for metronome
* @param vm
* @param block input parameter specifying whether caller should block if another request is ongoing
* @param responsesRequired the number of mutator threads that must voluntarily quiesce themselves
* @param gcPriority returned the new gc collector priority
* @return effectively a boolean the request is successful or not
*/
UDATA
requestExclusiveVMAccessMetronome(J9JavaVM *vm, UDATA block, UDATA *responsesRequired, UDATA *gcPriority);

/**
* @brief initiates an exclusive access request for metronome
* @param vm
* @param block input parameter specifying whether caller should block if another request is ongoing
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
registerNativeLibrary(J9VMThread *vmThread, J9ClassLoader *classLoader, const char *libName, const char *libraryPath, J9NativeLibrary **libraryPtr, char *errorBuffer, UDATA bufferLength);


/**
 * Initialize native library callback functions to default values.
 * \param javaVM
 * \param library The library to initialize.
 * \return Zero on success, non-zero on failure.
 */
UDATA
initializeNativeLibrary(J9JavaVM * javaVM, J9NativeLibrary* library);


#if defined(J9VM_ZOS_3164_INTEROPERABILITY) && (JAVA_SPEC_VERSION >= 17)
/**
 * Invoke JNI_OnLoad/JNI_OnUnload functions in 31-bit interoperability native target library.
 * @param vm The J9JavaVM pointer passed as first parameter to JNI_OnXLoad function
 * @param handle The target function pointer to invoke - should be a 31-bit interop target
 * @param isOnLoad JNI_TRUE if invoking JNI_OnLoad, JNI_FALSE if invoking JNI_OnUnload
 * @param reserved The reserved second parameter to JNI_OnXLoad function
 * @return the return value for JNI_OnLoad, or 0 for JNI_OnUnload
 */
I_32
invoke31BitJNI_OnXLoad(J9JavaVM *vm, void *handle, jboolean isOnLoad, void *reserved);
#endif /* defined(J9VM_ZOS_3164_INTEROPERABILITY) && (JAVA_SPEC_VERSION >= 17) */


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
 * @brief Retrieves the address of the default value slot for a value class
 * @param clazz The class to retrieve the default address value for. Must be an initialized value class.
 * @return the address of the default value slot for the value class
 */
j9object_t*
getDefaultValueSlotAddress(J9Class* clazz);

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
 * return a null-terminated modified UTF-8 version of charSequence.  Transliteration depends on the original encoding.
 * @param vm Java VM
 * @param userString system property value as provided by the user.  May contain embedded zero bytes.
 * @return copy of the string in Modified UTF-8, NULL if the string is invalid.
 * @note the return value must be freed by the caller.
 */
U_8*
getMUtf8String(J9JavaVM * vm, const char *userString, UDATA stringLength);

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


/* ---------------- vmthread.cpp ---------------- */

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
* @param *entryarg
* @return IDATA J9THREAD_PROC
*/
IDATA J9THREAD_PROC
javaThreadProc(void *entryarg);


/**
* @brief
* @param *vm
* @param *self
* @param *toFile
* @return void
*/
void
printThreadInfo(J9JavaVM *vm, J9VMThread *self, char *toFile, BOOLEAN allThreads);


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
 * @brief Returns the Java priority for the thread specified. For RealtimeThreads, the priority
 * comes from the priority field in PriorityParameters while for regular threads or any thread
 * in a non-realtime vm it comes from the priority field in Thread. Callers of this method must
 * ensure that the Java Thread object for the thread is not NULL and that this cannot change
 * while the call is being made. This method should also only be called when we don't need barrier
 * checks such as call from jvmti and ras.
 *
 * @param vm the vm used to lookup the priority
 * @param thread the thread for which the priority should be returned
 * @return UDATA
 */
UDATA
getJavaThreadPriority(struct J9JavaVM *vm, J9VMThread* thread );

#if JAVA_SPEC_VERSION >= 19
/* ---------------- ContinuationHelpers.cpp ---------------- */

/**
 * @brief Enters the Continuation runnable.
 * If the Continuation has not started, start the runnable task via callin to interpreter on the method Continuation.execute.
 * If the Continuation has already started, resume the execution with a new interpreter instance on the stack.
 *
 * @param currentThread the thread to mount Continuation.
 * @param continuationObject
 * @return BOOLEAN
 */
BOOLEAN
enterContinuation(struct J9VMThread *currentThread, j9object_t continuationObject);

/**
 * @brief Suspends the Continuation runnable.
 *
 * @param currentThread
 * @param isFinished true if it is last unmount
 * @return BOOLEAN
 */
BOOLEAN
yieldContinuation(struct J9VMThread *currentThread, BOOLEAN isFinished);

/**
 * @brief Free the native memory allocated by Continuation.
 *
 * @param currentThread the thread unmounting Continuation
 * @param continuationObject
 * @param skipLocalCache skip trying to store as local cache
 */
void
freeContinuation(J9VMThread *currentThread, j9object_t continuationObject, BOOLEAN skipLocalCache);

/**
 * @brief Recycle the native memory allocated by Continuation.
 *
 * @param vm
 * @param vmThread the thread unmounting Continuation
 * @param continuationObject
 * @param skipLocalCache skip trying to store as local cache
 */
void
recycleContinuation(J9JavaVM *vm, J9VMThread *vmThread, J9VMContinuation *continuation, BOOLEAN skipLocalCache);

/**
 * @brief Determine if the current continuation is pinned.
 *
 * @param currentThread
 * @return 0 if not pinned; otherwise, an error code corresponding to the pinned reason
 */
jint
isPinnedContinuation(J9VMThread *currentThread);

/**
 * @brief Copy data from Continuation struct to vmThread.
 *
 * @param currentThread
 * @param allocated J9VMThread pointer
 * @param allocated J9VMEntryLocalStorage pointer
 * @param continuation struct
 */
void
copyFieldsFromContinuation(J9VMThread *currentThread, J9VMThread *vmThread, J9VMEntryLocalStorage *els, J9VMContinuation *continuation);

/**
 * @brief Walk the stackframes associated with a continuation.
 *
 * @param currentThread current thread
 * @param continuation the continuation to be walked
 * @param threadObject the thread object whose state is stored in the continuation
 * @return 0 on success and non-zero on failure
 */
UDATA
walkContinuationStackFrames(J9VMThread *currentThread, J9VMContinuation *continuation, j9object_t threadObject, J9StackWalkState *walkState);

/**
 * @brief Walk all stackframes in the VM.
 *
 * @param currentThread
 * @param walkState walkstate holding initial walk parameters to be used in each stackwalk
 * @return 0 on success and non-zero on failure
 */
UDATA
walkAllStackFrames(J9VMThread *currentThread, J9StackWalkState *walkState);

/**
 * @brief Acquire inspector access on VirtualThread, block until access is acquired.
 *
 * @param currentThread
 * @param thread target VirtualThread to acquire the inspector on
 * @param spin call should spin until sucessfully acquired access
 * @return TRUE on success, FALSE on failure
 */
BOOLEAN
acquireVThreadInspector(J9VMThread *currentThread, jobject thread, BOOLEAN spin);

/**
 * @brief Release the inspector acquired from VirtualThread.
 *
 * @param currentThread
 * @param thread target VirtualThread to release the inspector on
 */
void
releaseVThreadInspector(J9VMThread *currentThread, jobject thread);
#endif /* JAVA_SPEC_VERSION >= 19 */

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
* @return J9ROMMethod
*
* Returns the method, or NULL on failure.
*/
J9ROMMethod *
	findROMMethodInROMClass(J9VMThread *vmThread, J9ROMClass *romClass, UDATA methodPC);

/**
* @brief Finds the rom class given a PC.  Also returns the classloader it belongs to.
* @param vmThread
* @param methodPC
* @param resultClassLoader  This is the classLoader that owns the memory segment that the ROMClass was found in.
*	For classes from the SharedClasses cache, this will always be the vm->systemClassLoader regardless of which
*	J9ClassLoader actually loaded the class.
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
 * @brief See if an object is being waited on by targetThread
 * @param currentThread
 * @param targetThread
 * @param obj
 * @return BOOLEAN
 */
BOOLEAN
objectIsBeingWaitedOn(J9VMThread *currentThread, J9VMThread *targetThread, j9object_t obj);

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
 * @param clazz
 * @param vmThread
 * @return UDATA
 */
UDATA
getVTableOffsetForMethod(J9Method * method, J9Class *clazz, J9VMThread *vmThread);

j9object_t
resolveMethodTypeRef(J9VMThread *vmThread, J9ConstantPool *constantPool, UDATA cpIndex, UDATA resolveFlags);

j9object_t
resolveMethodHandleRef(J9VMThread *vmThread, J9ConstantPool *constantPool, UDATA cpIndex, UDATA resolveFlags);

/* ---------------- logsupport.c ---------------- */

UDATA
queryLogOptions (J9JavaVM *vm, I_32 buffer_size, void *options, I_32 *data_size);

UDATA
setLogOptions (J9JavaVM *vm, char *options);

/* -------------------- NativeHelpers.cpp ------------ */

#if defined(J9VM_OPT_METHOD_HANDLE)
/**
* @brief
* @param currentThread
* @param methodType
* @return J9SFMethodTypeFrame The frame
*/
J9SFMethodTypeFrame *
buildMethodTypeFrame(J9VMThread * currentThread, j9object_t methodType);
#endif /* defined(J9VM_OPT_METHOD_HANDLE) */

/* -------------------- drophelp.c ------------ */

/**
* @brief Set the SP to the unwindSP (as if we had stack walked) of the current frame.
* Keep literals consistent with that.
* @param currentThread
* @return UDATA TRUE or FALSE - is this a bytecoded frame (i.e. can't push on top of it)?
*/
UDATA
dropPendingSendPushes(J9VMThread *currentThread);

/**
* @brief Prepare the current stack for throwing an exception (clear all pending, build necessary frames)
* @param currentThread
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

/* -------------------- threadpark.cpp ------------ */

/**
 * @param[in] vmThread the current thread
 * @param[in] timeoutIsEpochRelative is the timeout in milliseconds relative to the beginning of the epoch
 * @param[in] timeout nanosecond or millisecond timeout
 */
void
threadParkImpl(J9VMThread *vmThread, BOOLEAN timeoutIsEpochRelative, I_64 timeout);
void
threadUnparkImpl (J9VMThread* vmThread, j9object_t threadObject);

/* -------------------- threadhelp.cpp ------------ */

/**
 * A time compensation helper for Object.wait(), Thread.sleep(), and Unsafe.park().
 * No time compensation for these APIs if CRIU is disabled.
 *
 * @param[in] vmThread the current thread
 * @param[in] threadHelperType the helper type
 *            HELPER_TYPE_MONITOR_WAIT_INTERRUPTABLE - omrthread_monitor_wait_interruptable
 *            HELPER_TYPE_MONITOR_WAIT_TIMED         - omrthread_monitor_wait_timed
 *            HELPER_TYPE_THREAD_PARK                - omrthread_park
 *            HELPER_TYPE_THREAD_SLEEP               - omrthread_sleep_interruptable
 * @param[in] monitor the object monitor waiting on
 * @param[in] millis milliseconds timeout
 * @param[in] nanos nanosecond timeout
 */
IDATA
timeCompensationHelper(J9VMThread *vmThread, U_8 threadHelperType, omrthread_monitor_t monitor, I_64 millis, I_32 nanos);

/**
 * @param[in] vmThread current thread
 * @param[in] object the object to wait on
 * @param[in] millis millisecond timeout
 * @param[in] nanos nanosecond timeout
 * @param[in] interruptable set to FALSE to ignore interrupts
 *
 * @return 0 on success, non-zero on failure. This function always sets the current exception on failure.
 */
IDATA
monitorWaitImpl(J9VMThread *vmThread, j9object_t object, I_64 millis, I_32 nanos, BOOLEAN interruptable);

/**
 * @param[in] vmThread current thread
 * @param[in] millis millisecond timeout
 * @param[in] nanos nanosecond timeout
 *
 * @return 0 on success, non-zero on failure. This function always sets the current exception on failure.
 */
IDATA
threadSleepImpl(J9VMThread *vmThread, I_64 millis, I_32 nanos);

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
 * Helper function to create a joinable thread with a specific thread category.
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
createJoinableThreadWithCategory(omrthread_t* handle, UDATA stacksize, UDATA priority, UDATA suspend,
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

#if JAVA_SPEC_VERSION >= 19
/**
 * Frees a thread object's TLS array.
 *
 * @param[in] currentThread the current thread
 * @param[in] threadObj the thread object to free TLS from
 */
void
freeTLS(J9VMThread *currentThread, j9object_t threadObj);
#endif /* JAVA_SPEC_VERSION >= 19 */

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

#if JAVA_SPEC_VERSION >= 19
/**
 * Pop off the Callin frame from stack and update thread states.
 *
 * @param currentThread
 */
void
restoreCallInFrame(J9VMThread *currentThread);
#endif /* JAVA_SPEC_VERSION >= 19 */

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


/* FlushProcessWriteBuffers.cpp */

void flushProcessWriteBuffers(J9JavaVM *vm);

/* throwexception.c */
void
throwNativeOOMError(JNIEnv *env, U_32 moduleName, U_32 messageNumber);
void
throwNewJavaIoIOException(JNIEnv *env, const char *message);

#if JAVA_SPEC_VERSION >= 16

/* ------------------- UpcallThunkGen.cpp ----------------- */

/**
 * @brief Generate the appropriate thunk/adaptor for a given J9UpcallMetaData
 *
 * @param metaData[in/out] a pointer to the given J9UpcallMetaData
 * @return the address for this future upcall function handle, either the thunk or the thunk-descriptor
 */
void *
createUpcallThunk(J9UpcallMetaData *data);

/**
 * @brief Calculate the requested argument in-stack memory address to return
 *
 * @param nativeSig a pointer to the J9UpcallNativeSignature
 * @param argListPtr a pointer to the argument list prepared by the thunk
 * @param argIdx the requested argument index
 * @return the address in argument list for the requested argument
 *
 * Details:
 *   A quick walk-through of the argument list ahead of the requested one
 *   Calculating its address based on argListPtr
 */
void *
getArgPointer(J9UpcallNativeSignature *nativeSig, void *argListPtr, int argIdx);

/**
 * @brief Allocate a piece of thunk memory with a given size from the existing virtual memory block
 *
 * @param data a pointer to J9UpcallMetaData
 * @return the start address of the upcall thunk memory, or NULL on failure.
 */
void *
allocateUpcallThunkMemory(J9UpcallMetaData *data);


/**
 * @brief Flush the generated thunk to the memory
 *
 * @param data a pointer to J9UpcallMetaData
 * @param thunkAddress the address of the generated thunk
 * @return void
 */
void
doneUpcallThunkGeneration(J9UpcallMetaData *data, void *thunkAddress);

/* ------------------- UpcallVMHelpers.cpp ----------------- */

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and ignore the return value in the case of void.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return void
 */
void JNICALL
native2InterpJavaUpcall0(J9UpcallMetaData *data, void *argsListPointer);

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return an I_32 value in the case of byte/char/short/int.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return an I_32 value
 */
I_32 JNICALL
native2InterpJavaUpcall1(J9UpcallMetaData *data, void *argsListPointer);

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return an I_64 value in the case of long/pointer.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return an I_64 value
 */
I_64 JNICALL
native2InterpJavaUpcallJ(J9UpcallMetaData *data, void *argsListPointer);

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return a float value as specified in the return type.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return a float
 */
float JNICALL
native2InterpJavaUpcallF(J9UpcallMetaData *data, void *argsListPointer);

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return a double value as specified in the return type.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return a double
 */
double JNICALL
native2InterpJavaUpcallD(J9UpcallMetaData *data, void *argsListPointer);

/**
 * @brief Call into the interpreter via native2InterpJavaUpcallImpl to invoke the upcall
 * specific method handle and return a U_8 pointer to the requested struct.
 *
 * @param data a pointer to J9UpcallMetaData
 * @param argsListPointer a pointer to the argument list
 * @return a U_8 pointer
 */
U_8 * JNICALL
native2InterpJavaUpcallStruct(J9UpcallMetaData *data, void *argsListPointer);

#endif /* JAVA_SPEC_VERSION >= 16 */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* vm_api_h */
